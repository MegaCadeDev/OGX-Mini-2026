// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Xbox Wireless Adapter dongle logic, adapted from xone (transport/dongle.c).
 * Copyright (C) 2021 Severin von Wnuck-Lipinski <severinvonw@outlook.de>
 */

#include "dongle_core.h"

#include "mt76.h"
#include "tusb.h"
#include "pico/stdlib.h"

#define DL_LEN_CMD_PKT 0x0654
#define DL_LEN_WLAN_PKT 0x0800
#define DL_QUEUE_SLOTS 16
#define DL_QUEUE_BUF 0x0800
#define DL_PAIRING_TIMEOUT_MS 60000u

extern const uint8_t xbox_dongle_fw[];
extern const size_t xbox_dongle_fw_len;

struct dl_client {
    bool used;
    uint8_t wcid;
    uint8_t address[ETH_ALEN];
    bool encryption_enabled;
};

struct dl_msg {
    uint16_t len;
    uint8_t ep;
    uint8_t data[DL_QUEUE_BUF];
};

static struct xone_mt76 g_mt;
static struct usb_device g_udev;
static struct device g_dev;
static struct dl_client g_clients[DONGLE_CORE_MAX_CLIENTS];
static bool g_pairing;
static uint32_t g_pairing_deadline;
static bool g_ready;
static uint8_t g_gip_seq = 1;

static struct dl_msg g_queue[DL_QUEUE_SLOTS];
static volatile uint8_t g_q_head;
static volatile uint8_t g_q_tail;
static volatile uint32_t g_q_dropped;

static uint8_t g_buf_cmd[DL_LEN_CMD_PKT] __attribute__((aligned(4)));
static uint8_t g_buf_wlan[DL_LEN_WLAN_PKT] __attribute__((aligned(4)));
static volatile bool g_in_active;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

static void submit_in(uint8_t ep);

static void in_cb(tuh_xfer_t* x) {
    if (x->result == XFER_RESULT_SUCCESS && x->actual_len) {
        uint8_t next = (uint8_t)((g_q_head + 1) % DL_QUEUE_SLOTS);
        if (next == g_q_tail) {
            g_q_dropped++;
        } else {
            struct dl_msg* m = &g_queue[g_q_head];
            const uint8_t* src = ((x->ep_addr & 0x7f) == XONE_MT_EP_IN_CMD) ? g_buf_cmd : g_buf_wlan;
            uint32_t n = x->actual_len > DL_QUEUE_BUF ? DL_QUEUE_BUF : x->actual_len;
            memcpy(m->data, src, n);
            m->len = (uint16_t)n;
            m->ep = x->ep_addr;
            g_q_head = next;
        }
    }
    if (g_in_active && x->result != XFER_RESULT_STALLED) submit_in(x->ep_addr);
}

static void submit_in(uint8_t ep) {
    bool cmd = (ep & 0x7f) == XONE_MT_EP_IN_CMD;
    tuh_xfer_t x = {
        .daddr = g_udev.daddr,
        .ep_addr = ep,
        .buflen = cmd ? sizeof(g_buf_cmd) : sizeof(g_buf_wlan),
        .buffer = cmd ? g_buf_cmd : g_buf_wlan,
        .complete_cb = in_cb,
        .user_data = 0,
    };
    if (!tuh_edpt_xfer(&x)) kc_log("submit IN 0x%02X failed\r\n", ep);
}

static bool open_bulk(uint8_t ep) {
    tusb_desc_endpoint_t d = {
        .bLength = sizeof(tusb_desc_endpoint_t), .bDescriptorType = TUSB_DESC_ENDPOINT,
        .bEndpointAddress = ep, .bmAttributes = { .xfer = TUSB_XFER_BULK }, .wMaxPacketSize = 64, .bInterval = 0,
    };
    return tuh_edpt_open(g_udev.daddr, &d);
}

static int client_count(void) {
    int c = 0;
    for (int i = 0; i < DONGLE_CORE_MAX_CLIENTS; i++) if (g_clients[i].used) c++;
    return c;
}

static void set_pairing(bool enable) {
    if (g_pairing == enable) return;
    int err = xone_mt76_set_pairing(&g_mt, enable);
    if (err) { kc_log("set_pairing(%d) failed: %d\r\n", enable, err); return; }
    enum xone_mt76_led_mode led = enable ? XONE_MT_LED_BLINK : (client_count() ? XONE_MT_LED_ON : XONE_MT_LED_OFF);
    err = xone_mt76_set_led_mode(&g_mt, led);
    if (err) kc_log("set_led_mode failed: %d\r\n", err);
    g_pairing = enable;
    if (enable) g_pairing_deadline = now_ms() + DL_PAIRING_TIMEOUT_MS;
    kc_log("pairing %s\r\n", enable ? "on" : "off");
}

static struct dl_client* client_by_wcid(uint8_t wcid) {
    if (!wcid || wcid > DONGLE_CORE_MAX_CLIENTS) return NULL;
    struct dl_client* c = &g_clients[wcid - 1];
    return c->used ? c : NULL;
}

static void remove_client(uint8_t wcid);

static void add_client(const uint8_t* addr) {
    for (int i = 0; i < DONGLE_CORE_MAX_CLIENTS; i++) {
        if (g_clients[i].used && ether_addr_equal(g_clients[i].address, addr)) {
            kc_log("client wcid %u reconnecting, re-associating", g_clients[i].wcid);
            remove_client(g_clients[i].wcid);
            break;
        }
    }
    int slot = -1;
    for (int i = 0; i < DONGLE_CORE_MAX_CLIENTS; i++) if (!g_clients[i].used) { slot = i; break; }
    if (slot < 0) { kc_log("no free client slot\r\n"); return; }
    uint8_t wcid = (uint8_t)(slot + 1);
    uint8_t a[ETH_ALEN];
    memcpy(a, addr, ETH_ALEN);
    int err = xone_mt76_associate_client(&g_mt, wcid, a);
    if (err) { kc_log("associate failed: %d\r\n", err); return; }
    struct dl_client* c = &g_clients[slot];
    memset(c, 0, sizeof(*c));
    c->used = true;
    c->wcid = wcid;
    memcpy(c->address, addr, ETH_ALEN);
    if (!g_pairing) xone_mt76_set_led_mode(&g_mt, XONE_MT_LED_ON);
    kc_log("client wcid %u associated\r\n", wcid);
    dongle_core_on_client(wcid, true);
}

static void remove_client(uint8_t wcid) {
    struct dl_client* c = client_by_wcid(wcid);
    if (!c) return;
    c->used = false;
    int err = xone_mt76_remove_client(&g_mt, wcid);
    kc_log("client wcid %u removed%s\r\n", wcid, err ? " (remove failed)" : "");
    dongle_core_on_client(wcid, false);
    if (!client_count()) set_pairing(true);
}

static void pair_client(const uint8_t* addr) {
    uint8_t a[ETH_ALEN];
    memcpy(a, addr, ETH_ALEN);
    int err = xone_mt76_pair_client(&g_mt, a);
    kc_log("pair request %s\r\n", err ? "failed" : "accepted");
    if (!err) set_pairing(false);
}

static void enable_encryption(uint8_t wcid) {
    struct dl_client* c = client_by_wcid(wcid);
    if (!c) return;
    uint8_t data[] = { 0x00, 0x00 };
    int err = xone_mt76_send_client_command(&g_mt, wcid, c->address, XONE_MT_CLIENT_ENABLE_ENCRYPTION, data, sizeof(data));
    if (!err) c->encryption_enabled = true;
}

static void process_frame(struct sk_buff* skb, unsigned int hdr_len, uint8_t wcid) {
    struct ieee80211_hdr_3addr* hdr = (struct ieee80211_hdr_3addr*)skb->data;
    if (skb->len < hdr_len || hdr_len < sizeof(*hdr)) return;
    uint8_t addr2[ETH_ALEN];
    memcpy(addr2, hdr->addr2, ETH_ALEN);
    uint16_t type = le16_to_cpu(hdr->frame_control);
    skb_pull(skb, hdr_len);
    switch (type & (IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) {
    case IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA:
        if (client_by_wcid(wcid)) dongle_core_on_gip(wcid, skb->data, skb->len);
        return;
    case IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ASSOC_REQ:
        if (is_valid_ether_addr(addr2)) add_client(addr2);
        return;
    case IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_DISASSOC:
        remove_client(wcid);
        return;
    case IEEE80211_FTYPE_MGMT | XONE_MT_WLAN_RESERVED:
        if (skb->len < 2 || skb->data[0] != XONE_MT_WLAN_RESERVED) return;
        if (skb->data[1] == XONE_MT_CLIENT_PAIR_REQ) pair_client(addr2);
        else if (skb->data[1] == XONE_MT_CLIENT_ENABLE_ENCRYPTION) enable_encryption(wcid);
        return;
    }
}

static void process_wlan(struct sk_buff* skb) {
    struct mt76_rxwi* rxwi = (struct mt76_rxwi*)skb->data;
    if (skb->len < sizeof(*rxwi)) return;
    uint32_t rxinfo = le32_to_cpu(rxwi->rxinfo);
    uint32_t ctl = le32_to_cpu(rxwi->ctl);
    skb_pull(skb, sizeof(*rxwi));
    unsigned int hdr_len = ieee80211_get_hdrlen_from_skb(skb);
    if (!hdr_len) return;
    if (rxinfo & MT_RXINFO_L2PAD) {
        if (skb->len < hdr_len + 2) return;
        memmove(skb->data + 2, skb->data, hdr_len);
        skb_pull(skb, 2);
    }
    if (FIELD_GET(MT_RXWI_CTL_MPDU_LEN, ctl) > skb->len) return;
    skb_trim(skb, FIELD_GET(MT_RXWI_CTL_MPDU_LEN, ctl));
    process_frame(skb, hdr_len, (uint8_t)FIELD_GET(MT_RXWI_CTL_WCID, ctl));
}

static void process_message(struct sk_buff* skb) {
    if (skb->len < MT_CMD_HDR_LEN * 2) return;
    uint32_t info = get_unaligned_le32(skb->data);
    uint32_t port = FIELD_GET(MT_RX_FCE_INFO_D_PORT, info);
    if (FIELD_GET(MT_RX_FCE_INFO_CMD_SEQ, info) == 0x01) return;
    skb_pull(skb, MT_CMD_HDR_LEN);
    skb_trim(skb, skb->len - MT_CMD_HDR_LEN);
    if (port == MT_WLAN_PORT) { process_wlan(skb); return; }
    if (port != MT_CPU_RX_PORT) return;
    switch (FIELD_GET(MT_RX_FCE_INFO_EVT_TYPE, info)) {
    case XONE_MT_EVT_BUTTON:
        set_pairing(true);
        return;
    case XONE_MT_EVT_PACKET_RX:
        process_wlan(skb);
        return;
    case XONE_MT_EVT_CLIENT_LOST:
        if (skb->len >= 1) remove_client(skb->data[0]);
        return;
    default:
        return;
    }
}

bool dongle_core_start(uint8_t daddr) {
    memset(&g_mt, 0, sizeof(g_mt));
    memset(g_clients, 0, sizeof(g_clients));
    g_udev.daddr = daddr;
    g_mt.udev = &g_udev;
    g_mt.dev = &g_dev;
    g_pairing = false;
    g_ready = false;
    g_q_head = g_q_tail = 0;
    g_q_dropped = 0;

    struct firmware fw = { .size = xbox_dongle_fw_len, .data = xbox_dongle_fw };
    uint32_t dma = 0;
    usb_control_msg(&g_udev, 0x80, MT_VEND_READ_CFG, USB_DIR_IN | USB_TYPE_VENDOR, 0, MT_FCE_DMA_ADDR, &dma, sizeof(dma), 1000);
    kc_log("FCE_DMA_ADDR before load 0x%08lx, fw %u bytes", (unsigned long)dma, (unsigned)xbox_dongle_fw_len);
    int err = -ETIMEDOUT;
    for (int i = 0; i < 3 && err; i++) {
        err = xone_mt76_load_firmware(&g_mt, &fw);
        if (err == -ETIMEDOUT) {
            dma = 0;
            usb_control_msg(&g_udev, 0x80, MT_VEND_READ_CFG, USB_DIR_IN | USB_TYPE_VENDOR, 0, MT_FCE_DMA_ADDR, &dma, sizeof(dma), 1000);
            if (dma & 0x01) {
                kc_log("firmware reset timed out but firmware is running (0x%08lx)", (unsigned long)dma);
                kc_msleep(500);
                err = 0;
                break;
            }
        }
        if (err) { kc_log("load firmware attempt %d failed: %d", i + 1, err); kc_msleep(1000); }
    }
    if (err) return false;
    kc_log("firmware running");

    if (!open_bulk(0x80 | XONE_MT_EP_IN_CMD) || !open_bulk(0x80 | XONE_MT_EP_IN_WLAN)) {
        kc_log("open bulk endpoints failed\r\n");
        return false;
    }
    g_in_active = true;
    submit_in(0x80 | XONE_MT_EP_IN_CMD);
    submit_in(0x80 | XONE_MT_EP_IN_WLAN);

    err = -ETIMEDOUT;
    for (int i = 0; i < 3 && err == -ETIMEDOUT; i++) {
        err = xone_mt76_init_radio(&g_mt);
        if (err == -ETIMEDOUT) kc_msleep(500);
    }
    if (err) { kc_log("init_radio failed: %d\r\n", err); g_in_active = false; return false; }

    kc_log("radio up on channel %u\r\n", g_mt.channel ? g_mt.channel->index : 0);
    g_ready = true;
    set_pairing(true);
    return true;
}

void dongle_core_stop(void) {
    g_in_active = false;
    g_ready = false;
    for (uint8_t i = 0; i < DONGLE_CORE_MAX_CLIENTS; i++) {
        if (g_clients[i].used) {
            g_clients[i].used = false;
            dongle_core_on_client((uint8_t)(i + 1), false);
        }
    }
}

bool dongle_core_ready(void) { return g_ready; }

void dongle_core_task(void) {
    while (g_q_tail != g_q_head) {
        struct dl_msg* m = &g_queue[g_q_tail];
        struct sk_buff skb = { .head = m->data, .data = m->data, .len = m->len, .size = DL_QUEUE_BUF };
        if (g_ready) process_message(&skb);
        g_q_tail = (uint8_t)((g_q_tail + 1) % DL_QUEUE_SLOTS);
    }
    if (g_q_dropped) {
        kc_log("%lu IN messages dropped\r\n", (unsigned long)g_q_dropped);
        g_q_dropped = 0;
    }
    if (g_ready && g_pairing && client_count() && (int32_t)(now_ms() - g_pairing_deadline) >= 0) set_pairing(false);
}

void dongle_core_request_pairing(void) {
    if (g_ready) set_pairing(true);
}

uint8_t dongle_core_next_sequence(void) {
    while (!g_gip_seq) g_gip_seq++;
    return g_gip_seq++;
}

int dongle_core_send_gip(uint8_t wcid, const uint8_t* pkt, uint32_t len) {
    struct dl_client* c = client_by_wcid(wcid);
    if (!c || !g_ready) return -ENODEV;
    struct sk_buff* skb = xone_mt76_alloc_message(DL_LEN_CMD_PKT, GFP_KERNEL);
    if (!skb) return -ENOMEM;
    skb_reserve(skb, MT_CMD_HDR_LEN + 8 + sizeof(struct mt76_txwi) + sizeof(struct ieee80211_qos_hdr) + 2 + MT_CMD_HDR_LEN);
    skb_put_data(skb, pkt, len);

    struct ieee80211_qos_hdr hdr = {0};
    struct mt76_txwi txwi = {0};
    uint8_t info[] = { 0x00, 0x00, 0x00, (uint8_t)(c->wcid - 1), 0x00, 0x00, 0x00, 0x00 };
    hdr.frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA | IEEE80211_FCTL_FROMDS);
    if (c->encryption_enabled) hdr.frame_control |= cpu_to_le16(IEEE80211_FCTL_PROTECTED);
    hdr.duration_id = cpu_to_le16(144);
    memcpy(hdr.addr1, c->address, ETH_ALEN);
    memcpy(hdr.addr2, g_mt.address, ETH_ALEN);
    memcpy(hdr.addr3, g_mt.address, ETH_ALEN);
    txwi.flags = cpu_to_le16(FIELD_PREP(MT_TXWI_FLAGS_MPDU_DENSITY, IEEE80211_HT_MPDU_DENSITY_4));
    txwi.rate = cpu_to_le16(FIELD_PREP(MT_RXWI_RATE_PHY, MT_PHY_TYPE_OFDM));
    txwi.ack_ctl = MT_TXWI_ACK_CTL_REQ;
    txwi.wcid = c->wcid - 1;
    txwi.len_ctl = cpu_to_le16(sizeof(hdr) + skb->len);
    memset(skb_push(skb, 2), 0, 2);
    memcpy(skb_push(skb, sizeof(hdr)), &hdr, sizeof(hdr));
    memcpy(skb_push(skb, sizeof(txwi)), &txwi, sizeof(txwi));
    memcpy(skb_push(skb, sizeof(info)), info, sizeof(info));
    xone_mt76_prep_command(skb, 0);

    int err = usb_bulk_msg(g_mt.udev, usb_sndbulkpipe(g_mt.udev, XONE_MT_EP_OUT), skb->data, (int)skb->len, NULL, 1000);
    kfree_skb(skb);
    return err;
}
