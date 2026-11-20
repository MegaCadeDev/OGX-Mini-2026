// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal Linux kernel shim so xone's transport/mt76.c builds on the RP2040.
 */

#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kc_glue.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
typedef unsigned int gfp_t;

#define GFP_KERNEL 0u
#define GFP_ATOMIC 0u
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef EREMOTEIO
#define EREMOTEIO 121
#endif

#define BIT(n) (1u << (n))
#define GENMASK(h, l) ((uint32_t)(((~0u) << (l)) & (~0u >> (31 - (h)))))
#define FIELD_PREP(mask, val) ((uint32_t)(((uint32_t)(val) << __builtin_ctz(mask)) & (mask)))
#define FIELD_GET(mask, reg) ((uint32_t)(((uint32_t)(reg) & (mask)) >> __builtin_ctz(mask)))

#define cpu_to_le16(x) ((uint16_t)(x))
#define cpu_to_le32(x) ((uint32_t)(x))
#define le16_to_cpu(x) ((uint16_t)(x))
#define le32_to_cpu(x) ((uint32_t)(x))

#define min_t(t, a, b) ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define round_up(x, y) roundup(x, y)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static inline void put_unaligned_le32(uint32_t v, void* p) {
    uint8_t* b = (uint8_t*)p;
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}

static inline uint32_t get_unaligned_le32(const void* p) {
    const uint8_t* b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

#define ETH_ALEN 6

static inline bool ether_addr_equal(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, ETH_ALEN) == 0; }
static inline bool is_zero_ether_addr(const char* a) {
    for (int i = 0; i < ETH_ALEN; i++) if (a[i]) return false;
    return true;
}
static inline bool is_valid_ether_addr(const uint8_t* a) {
    return !(a[0] & 0x01) && !is_zero_ether_addr((const char*)a);
}
static inline void eth_broadcast_addr(uint8_t* a) { memset(a, 0xff, ETH_ALEN); }


#define msleep(ms) kc_msleep(ms)
#define usleep_range(lo, hi) kc_msleep(((hi) + 999) / 1000)
#define ssleep(s) kc_msleep((s) * 1000)

struct device { int unused; };
#define dev_dbg(dev, fmt, ...) do { } while (0)
#define dev_err(dev, fmt, ...) kc_log("[ERR] " fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) kc_log("[warn] " fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) kc_log("[info] " fmt, ##__VA_ARGS__)

#define module_param_array(...)
#define MODULE_PARM_DESC(...)

struct sk_buff {
    uint8_t* head;
    uint8_t* data;
    uint32_t len;
    uint32_t size;
    uint8_t cb[48];
};

static inline struct sk_buff* alloc_skb(unsigned int size, gfp_t gfp) {
    (void)gfp;
    struct sk_buff* skb = (struct sk_buff*)calloc(1, sizeof(struct sk_buff));
    if (!skb) return NULL;
    skb->head = (uint8_t*)calloc(1, size ? size : 1);
    if (!skb->head) { free(skb); return NULL; }
    skb->data = skb->head;
    skb->size = size;
    return skb;
}
static inline void kfree_skb(struct sk_buff* skb) { if (skb) { free(skb->head); free(skb); } }
#define consume_skb(s) kfree_skb(s)
static inline uint32_t skb_headroom(const struct sk_buff* s) { return (uint32_t)(s->data - s->head); }
static inline uint32_t skb_tailroom(const struct sk_buff* s) { return s->size - skb_headroom(s) - s->len; }
static inline void skb_reserve(struct sk_buff* s, uint32_t n) { s->data += n; }
static inline void* skb_put(struct sk_buff* s, uint32_t n) {
    if (skb_tailroom(s) < n) { kc_log("[ERR] skb_put overflow %u > %u\r\n", (unsigned)n, (unsigned)skb_tailroom(s)); abort(); }
    uint8_t* t = s->data + s->len; s->len += n; return t;
}
static inline void* skb_put_data(struct sk_buff* s, const void* d, uint32_t n) { void* t = skb_put(s, n); memcpy(t, d, n); return t; }
static inline void skb_put_u8(struct sk_buff* s, uint8_t v) { *(uint8_t*)skb_put(s, 1) = v; }
static inline void* skb_push(struct sk_buff* s, uint32_t n) {
    if (skb_headroom(s) < n) { kc_log("[ERR] skb_push underflow %u > %u\r\n", (unsigned)n, (unsigned)skb_headroom(s)); abort(); }
    s->data -= n; s->len += n; return s->data;
}
static inline void* skb_pull(struct sk_buff* s, uint32_t n) { if (n > s->len) return NULL; s->data += n; s->len -= n; return s->data; }
static inline void skb_trim(struct sk_buff* s, uint32_t n) { if (n < s->len) s->len = n; }

struct firmware { size_t size; const uint8_t* data; };

#define USB_DIR_OUT 0x00
#define USB_DIR_IN 0x80
#define USB_TYPE_VENDOR 0x40
#define usb_sndctrlpipe(dev, ep) (0u)
#define usb_rcvctrlpipe(dev, ep) (0x80u)
#define usb_sndbulkpipe(dev, ep) ((unsigned)(ep))
#define usb_rcvbulkpipe(dev, ep) (0x80u | (unsigned)(ep))


#define IEEE80211_FCTL_FTYPE 0x000c
#define IEEE80211_FCTL_STYPE 0x00f0
#define IEEE80211_FCTL_TODS 0x0100
#define IEEE80211_FCTL_FROMDS 0x0200
#define IEEE80211_FCTL_ORDER 0x8000
#define IEEE80211_FCTL_PROTECTED 0x4000
#define IEEE80211_FTYPE_MGMT 0x0000
#define IEEE80211_FTYPE_CTL 0x0004
#define IEEE80211_FTYPE_DATA 0x0008
#define IEEE80211_STYPE_ASSOC_REQ 0x0000
#define IEEE80211_STYPE_ASSOC_RESP 0x0010
#define IEEE80211_STYPE_BEACON 0x0080
#define IEEE80211_STYPE_DISASSOC 0x00A0
#define IEEE80211_STYPE_QOS_DATA 0x0080
#define IEEE80211_HT_MPDU_DENSITY_4 5

struct ieee80211_hdr_3addr {
    __le16 frame_control;
    __le16 duration_id;
    uint8_t addr1[ETH_ALEN];
    uint8_t addr2[ETH_ALEN];
    uint8_t addr3[ETH_ALEN];
    __le16 seq_ctrl;
} __packed;

struct ieee80211_qos_hdr {
    __le16 frame_control;
    __le16 duration_id;
    uint8_t addr1[ETH_ALEN];
    uint8_t addr2[ETH_ALEN];
    uint8_t addr3[ETH_ALEN];
    __le16 seq_ctrl;
    __le16 qos_ctrl;
} __packed;

struct ieee80211_mgmt {
    __le16 frame_control;
    __le16 duration;
    uint8_t da[ETH_ALEN];
    uint8_t sa[ETH_ALEN];
    uint8_t bssid[ETH_ALEN];
    __le16 seq_ctrl;
    union {
        struct {
            __le64 timestamp;
            __le16 beacon_int;
            __le16 capab_info;
        } __packed beacon;
        struct {
            __le16 capab_info;
            __le16 status_code;
            __le16 aid;
        } __packed assoc_resp;
    } u;
} __packed;

static inline unsigned int ieee80211_hdrlen(uint16_t fc) {
    unsigned int hdrlen = 24;
    if ((fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_DATA) {
        if ((fc & (IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS)) == (IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS)) hdrlen = 30;
        if (fc & 0x0080) {
            hdrlen += 2;
            if (fc & IEEE80211_FCTL_ORDER) hdrlen += 4;
        }
        return hdrlen;
    }
    if ((fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_CTL) {
        if ((fc & 0x00e0) == 0x00c0) return 10;
        return 16;
    }
    return hdrlen;
}

static inline unsigned int ieee80211_get_hdrlen_from_skb(const struct sk_buff* skb) {
    if (skb->len < 10) return 0;
    unsigned int hdrlen = ieee80211_hdrlen((uint16_t)(skb->data[0] | (skb->data[1] << 8)));
    if (hdrlen > skb->len) return 0;
    return hdrlen;
}
