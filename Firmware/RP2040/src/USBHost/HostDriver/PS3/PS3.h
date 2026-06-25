#ifndef _PS3_HOST_H_
#define _PS3_HOST_H_

#include <cstdint>
#include <array>

#include "tusb.h"

#include "Descriptors/PS3.h"
#include "USBHost/HostDriver/HostDriver.h"

class PS3Host : public HostDriver
{
public:
    PS3Host(uint8_t idx)
        : HostDriver(idx) {}

    void initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) override;
    void process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len) override;
    bool send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance) override;

private:
    enum class InitStage { RESP1, RESP2, RESP3, DONE };

    struct InitState
    {
        uint8_t dev_addr{0xFF};
        InitStage stage{InitStage::RESP1};
        uint8_t init_retries{0};
        std::array<uint8_t, 17> init_buffer;
        std::array<uint8_t, 8> bt_pair_report{};
        PS3::OutReport* out_report{nullptr};
        uint8_t usb_instance{0};
        bool reports_enabled{false};
        /** If set, points to PS3Host::ds3_bt_pair_deferred_ — BT addr was not ready during init. */
        bool* defer_bt_pair{nullptr};
    };

    static constexpr uint8_t MAX_INIT_RETRIES = 3;

    static void send_init_get_report(InitState* init_state);
    static void init_host_out_report(PS3::OutReport& out, uint8_t player_idx);
    static bool send_control_xfer_wait(uint8_t dev_addr, const tusb_control_request_t* req, uint8_t* buffer,
                                       uint32_t max_attempts_ms);
    static bool send_control_output_sync(uint8_t address, PS3::OutReport* report);
    static bool send_control_output_async(uint8_t address, PS3::OutReport* report);

    static const tusb_control_request_t ENABLE_USB_REQUEST;
    static const tusb_control_request_t RUMBLE_REQUEST;
#if defined(CONFIG_EN_BLUETOOTH)
    /** HID feature report: program DualShock 3 master Bluetooth address (see Bluepad32 sixaxispairer). */
    static const tusb_control_request_t BT_MAC_FEATURE_REQUEST;
#endif

    PS3::InReport prev_in_report_;
    PS3::OutReport out_report_;
    InitState init_state_;
    uint32_t last_keepalive_ms_{0};
#if defined(CONFIG_EN_USB_HOST)
    static constexpr uint32_t KEEPALIVE_MS = 1000u;
#else
    static constexpr uint32_t KEEPALIVE_MS = 0u;
#endif
#if defined(CONFIG_EN_BLUETOOTH)
    /** Persistent buffer for async SET_REPORT (feature 0xF5). */
    std::array<uint8_t, 8> bt_pair_buf_{};
    bool ds3_bt_pair_deferred_{false};
#endif

    static bool send_control_xfer(uint8_t dev_addr, const tusb_control_request_t* req, uint8_t* buffer, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
    static void get_report_complete_cb(tuh_xfer_s *xfer);
#if defined(CONFIG_EN_BLUETOOTH)
    static void bt_pair_complete_cb(tuh_xfer_s* xfer);
    static bool local_bt_addr_is_nonzero(const uint8_t addr[6]);
    void try_deferred_ds3_bt_pair(uint8_t address);
#endif
};

#endif // _PS3_HOST_H_