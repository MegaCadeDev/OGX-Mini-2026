#include "USBHost/HostDriver/XboxDongle/tuh_xbox_dongle.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pio_usb.h"

#include "Board/board_api.h"
#include "Board/ogxm_log.h"
#include "Diagnostics/Diagnostics.h"
#include "OGXMini/OGXMini.h"
#include "USBHost/HostManager.h"

#include <cerrno>

#include "USBHost/HostDriver/XboxDongle/dongle_core.h"
#include "USBHost/HostDriver/XboxDongle/kc_glue.h"

namespace {

constexpr uint8_t INSTANCE = 0;
constexpr uint8_t GIP_CMD_ACK = 0x01;
constexpr uint8_t GIP_CMD_ANNOUNCE = 0x02;
constexpr uint8_t GIP_CMD_POWER = 0x05;
constexpr uint8_t GIP_CMD_VIRTUAL_KEY = 0x07;
constexpr uint8_t GIP_CMD_RUMBLE = 0x09;
constexpr uint8_t GIP_CMD_LED = 0x0a;
constexpr uint8_t GIP_CMD_INPUT = 0x20;
constexpr uint8_t GIP_OPT_ACK = 0x10;
constexpr uint8_t GIP_OPT_INTERNAL = 0x20;
constexpr uint16_t MIN_INPUT_LEN = 19;

enum class State : uint8_t { IDLE, NEED_START, RUNNING, FAILED };

struct Dongle {
    uint8_t daddr{0};
    uint8_t itf_num{0xFF};
    State state{State::IDLE};
    bool out_opened{false};
    bool driver_mounted{false};
    uint8_t active_wcid{0};
    std::array<bool, DONGLE_CORE_MAX_CLIENTS + 1> started{};
};

Dongle dongle_;

volatile bool bulk_done_;
volatile xfer_result_t bulk_result_;
volatile uint32_t bulk_actual_;

void bulk_cb(tuh_xfer_t* x) {
    bulk_result_ = x->result;
    bulk_actual_ = x->actual_len;
    bulk_done_ = true;
}

void service_host() {
    board_api::usb::service_host_frame();
    tuh_task();
}

int send_gip_cmd(uint8_t wcid, uint8_t cmd, uint8_t opts, uint8_t seq, const uint8_t* payload, uint8_t plen) {
    uint8_t pkt[4 + 32];
    if (plen > 32) {
        return -EINVAL;
    }
    pkt[0] = cmd;
    pkt[1] = opts;
    pkt[2] = seq ? seq : dongle_core_next_sequence();
    pkt[3] = plen;
    if (plen) {
        std::memcpy(pkt + 4, payload, plen);
    }
    return dongle_core_send_gip(wcid, pkt, 4u + plen);
}

void start_client(uint8_t wcid) {
    static constexpr uint8_t power_on[] = { 0x00 };
    static constexpr uint8_t led_on[] = { 0x00, 0x01, 0x14 };
    send_gip_cmd(wcid, GIP_CMD_POWER, GIP_OPT_INTERNAL, 0, power_on, sizeof(power_on));
    send_gip_cmd(wcid, GIP_CMD_LED, GIP_OPT_INTERNAL, 0, led_on, sizeof(led_on));
    dongle_.started[wcid] = true;
    kc_log("controller wcid %u started", wcid);
}

void mount_driver() {
    if (dongle_.driver_mounted) {
        return;
    }
    HostManager& hm = HostManager::get_instance();
    if (hm.setup_driver(HostDriverType::XBOX_DONGLE, HostManager::DriverClass::XINPUT, dongle_.daddr, INSTANCE)) {
        dongle_.driver_mounted = true;
        OGXMini::host_mounted(true);
    }
}

void unmount_driver() {
    if (!dongle_.driver_mounted) {
        return;
    }
    HostManager& hm = HostManager::get_instance();
    hm.deinit_driver(HostManager::DriverClass::XINPUT, dongle_.daddr, INSTANCE);
    dongle_.driver_mounted = false;
    if (!hm.any_mounted()) {
        OGXMini::host_mounted(false);
    }
}

bool init() {
    dongle_ = Dongle{};
    return true;
}

bool deinit() {
    return true;
}

bool open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const* desc_itf, uint16_t max_len) {
    (void)rhport;
    (void)max_len;
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    TU_VERIFY(tuh_xbox_dongle::is_supported(vid, pid));
    TU_VERIFY(desc_itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && desc_itf->bInterfaceNumber == 0);
    TU_VERIFY(dongle_.state == State::IDLE);
    dongle_.daddr = dev_addr;
    dongle_.itf_num = desc_itf->bInterfaceNumber;
    kc_log("claimed %04x:%04x addr %u", vid, pid, dev_addr);
    return true;
}

bool set_config(uint8_t dev_addr, uint8_t itf_num) {
    TU_VERIFY(dev_addr == dongle_.daddr && itf_num == dongle_.itf_num);
    dongle_.state = State::NEED_START;
    usbh_driver_set_config_complete(dev_addr, itf_num);
    mount_driver();
    return true;
}

bool xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)dev_addr;
    (void)ep_addr;
    (void)result;
    (void)xferred_bytes;
    return true;
}

void close(uint8_t dev_addr) {
    if (dev_addr != dongle_.daddr) {
        return;
    }
    dongle_core_stop();
    unmount_driver();
    dongle_ = Dongle{};
}

}

extern "C" {

void kc_log(const char* fmt, ...) {
    char line[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    size_t n = std::strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
        line[--n] = 0;
    }
    diag::event(board_api::ms_since_boot(), "dongle: %s", line);
    OGXM_LOG("%s\n", line);
}

void kc_msleep(uint32_t ms) {
    const uint32_t start = board_api::ms_since_boot();
    while (board_api::ms_since_boot() - start < ms) {
        service_host();
    }
}

int usb_control_msg(struct usb_device* dev, unsigned pipe, uint8_t request, uint8_t requesttype,
                    uint16_t value, uint16_t index, void* data, uint16_t size, int timeout) {
    (void)pipe;
    (void)timeout;
    tusb_control_request_t r = {};
    r.bmRequestType = requesttype;
    r.bRequest = request;
    r.wValue = value;
    r.wIndex = index;
    r.wLength = size;
    tuh_xfer_t x = {};
    x.daddr = dev->daddr;
    x.ep_addr = 0;
    x.setup = &r;
    x.buffer = static_cast<uint8_t*>(data);
    x.complete_cb = nullptr;
    x.user_data = 0;
    if (!tuh_control_xfer(&x) || x.result != XFER_RESULT_SUCCESS) {
        return -EIO;
    }
    if (size == 0) {
        return 0;
    }
    return static_cast<int>(x.actual_len > size ? size : x.actual_len);
}

int usb_bulk_msg(struct usb_device* dev, unsigned pipe, void* data, int len, int* actual, int timeout) {
    if ((pipe & 0x80u) != 0) {
        return -EINVAL;
    }
    const uint8_t ep = static_cast<uint8_t>(pipe & 0x7fu);
    if (!dongle_.out_opened) {
        tusb_desc_endpoint_t d = {};
        d.bLength = sizeof(tusb_desc_endpoint_t);
        d.bDescriptorType = TUSB_DESC_ENDPOINT;
        d.bEndpointAddress = ep;
        d.bmAttributes.xfer = TUSB_XFER_BULK;
        d.wMaxPacketSize = 64;
        d.bInterval = 0;
        if (!tuh_edpt_open(dev->daddr, &d)) {
            return -EIO;
        }
        dongle_.out_opened = true;
    }
    bulk_done_ = false;
    tuh_xfer_t x = {};
    x.daddr = dev->daddr;
    x.ep_addr = ep;
    x.buflen = static_cast<uint32_t>(len);
    x.buffer = static_cast<uint8_t*>(data);
    x.complete_cb = bulk_cb;
    x.user_data = 0;
    if (!tuh_edpt_xfer(&x)) {
        return -EBUSY;
    }
    const uint32_t start = board_api::ms_since_boot();
    const uint32_t limit = timeout > 0 ? static_cast<uint32_t>(timeout) : 1000u;
    while (!bulk_done_) {
        service_host();
        if (board_api::ms_since_boot() - start > limit) {
            tuh_edpt_abort_xfer(dev->daddr, ep);
            return -ETIMEDOUT;
        }
    }
    if (actual) {
        *actual = static_cast<int>(bulk_actual_);
    }
    return (bulk_result_ == XFER_RESULT_SUCCESS && bulk_actual_ == static_cast<uint32_t>(len)) ? 0 : -EIO;
}

void dongle_core_on_client(uint8_t wcid, bool connected) {
    if (wcid > DONGLE_CORE_MAX_CLIENTS) {
        return;
    }
    dongle_.started[wcid] = false;
    HostManager& hm = HostManager::get_instance();
    if (connected) {
        if (!dongle_.active_wcid) {
            dongle_.active_wcid = wcid;
            hm.connect_cb(HostManager::DriverClass::XINPUT, dongle_.daddr, INSTANCE);
        }
        return;
    }
    if (dongle_.active_wcid == wcid) {
        dongle_.active_wcid = 0;
        hm.disconnect_cb(HostManager::DriverClass::XINPUT, dongle_.daddr, INSTANCE);
    }
}

void dongle_core_on_gip(uint8_t wcid, const uint8_t* data, uint32_t len) {
    if (len < 4 || wcid > DONGLE_CORE_MAX_CLIENTS) {
        return;
    }
    const uint8_t cmd = data[0];
    const uint8_t opts = data[1];
    const uint8_t seq = data[2];
    const uint8_t plen = data[3];

    if (opts & GIP_OPT_ACK) {
        const uint8_t ack[9] = { 0x00, cmd, static_cast<uint8_t>(opts & 0x2f), plen, 0x00, 0x00, 0x00, 0x00, 0x00 };
        send_gip_cmd(wcid, GIP_CMD_ACK, GIP_OPT_INTERNAL, seq, ack, sizeof(ack));
    }
    if (cmd == GIP_CMD_ANNOUNCE) {
        if (!dongle_.active_wcid) {
            dongle_core_on_client(wcid, true);
        }
        if (!dongle_.started[wcid]) {
            start_client(wcid);
        }
        return;
    }
    if (wcid != dongle_.active_wcid) {
        return;
    }
    if (cmd != GIP_CMD_INPUT && cmd != GIP_CMD_VIRTUAL_KEY) {
        return;
    }
    uint8_t report[64] = {};
    const uint32_t n = len > sizeof(report) ? sizeof(report) : len;
    std::memcpy(report, data, n);
    const uint16_t report_len = static_cast<uint16_t>((cmd == GIP_CMD_INPUT && n < MIN_INPUT_LEN) ? MIN_INPUT_LEN : n);
    HostManager::get_instance().process_report(HostManager::DriverClass::XINPUT, dongle_.daddr, INSTANCE, report, report_len);
}

}

namespace tuh_xbox_dongle {

bool is_supported(uint16_t vid, uint16_t pid) {
    return vid == 0x045e && pid == 0x02fe;
}

void task() {
    if (dongle_.state == State::NEED_START) {
        kc_log("starting (mounted %d)", dongle_.driver_mounted ? 1 : 0);
        dongle_.state = dongle_core_start(dongle_.daddr) ? State::RUNNING : State::FAILED;
        kc_log("start %s", dongle_.state == State::RUNNING ? "ok" : "FAILED");
    }
    if (dongle_.state == State::RUNNING) {
        dongle_core_task();
    }
}

uint8_t active_wcid() {
    return dongle_.active_wcid;
}

bool send_rumble(uint8_t wcid, uint8_t rumble_l, uint8_t rumble_r) {
    if (!wcid || dongle_.state != State::RUNNING) {
        return false;
    }
    const uint8_t payload[9] = { 0x00, 0x0f, 0x00, 0x00,
                                 static_cast<uint8_t>(rumble_l / 2), static_cast<uint8_t>(rumble_r / 2),
                                 0xff, 0x00, 0xff };
    return send_gip_cmd(wcid, GIP_CMD_RUMBLE, 0x00, 0, payload, sizeof(payload)) == 0;
}

const usbh_class_driver_t* class_driver() {
    static const usbh_class_driver_t driver = {
#if CFG_TUSB_DEBUG >= 2
        .name = "XboxDongle",
#else
        .name = nullptr,
#endif
        .init = init,
        .deinit = deinit,
        .open = open,
        .set_config = set_config,
        .xfer_cb = xfer_cb,
        .close = close,
    };
    return &driver;
}

}
