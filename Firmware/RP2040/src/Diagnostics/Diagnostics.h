#ifndef _OGXM_DIAGNOSTICS_H_
#define _OGXM_DIAGNOSTICS_H_

#include <cstddef>
#include <cstdint>

/*  Diagnostics: what a user can send without a serial adapter.
 *
 *  Kept in RAM (no flash writes). For each Bluetooth controller: what it is (name,
 *  IDs, address prefix, BLE Device Information strings), its link (Classic or LE, LE interval /
 *  latency / timeout, link mode) and its input
 *  timing (reports per second, typical interval, late reports, largest gap, reports lost by the
 *  controller's own counter). The same timing for wired USB controllers, the USB output side, and
 *  a ring of recent events. The web app asks for it in Web App mode (GET_DIAGNOSTICS) and saves
 *  a report.
 *
 *  Producers run on both cores; every access takes one critical section.
 */
namespace diag {

    constexpr size_t kSlots = 4;          // >= CONFIG_BLUEPAD32_MAX_DEVICES (checked in Bluepad32.cpp)
    constexpr size_t kUsbDevices = 4;
    // Events kept, by chip (RAM: 256 KB on the RP2040, 512 KB on the RP2350); a build option
    // (OGXM_DIAG_EVENTS) can lower it for a board. The report fits about 14 KB (255 USB chunks).
#if defined(OGXM_DIAG_EVENTS)
    constexpr size_t kEvents = OGXM_DIAG_EVENTS;
#elif defined(PICO_RP2350)
    constexpr size_t kEvents = 96;
#else
    constexpr size_t kEvents = 64;
#endif
    constexpr size_t kEventText = 72;
    constexpr size_t kNameLength = 32;
    constexpr size_t kInfoText = 24;
    constexpr uint32_t kGapWindowMs = 5000;  // windows for gaps / late reports: last 5-10 s

    struct BoardInfo {
        const char* firmware_version;
        const char* board;
        const char* chip;
        uint32_t clock_mhz;
        const char* build_type;
        const char* output_mode;
        const char* reset_reason;
        uint8_t max_gamepads;
    };

    void init(const BoardInfo& info);

    // Recent events ring (oldest dropped). printf-style, truncated to kEventText.
    void event(uint32_t now_ms, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    // ---- Bluetooth controllers, by slot ----
    void slot_connected(size_t slot, uint32_t now_ms, const char* name, uint16_t vid, uint16_t pid,
                        uint8_t controller_type, bool le, uint16_t con_handle, const uint8_t address[6]);
    void slot_disconnected(size_t slot, uint32_t now_ms);
    void slot_report(size_t slot, uint32_t now_ms);
    // The controller's own report counter (bits wide); lost reports are the skipped values.
    void slot_counter(size_t slot, uint32_t value, uint8_t bits);
    void slot_battery(size_t slot, uint8_t level_0_255);

    // ---- Bluetooth link, by connection handle (may arrive before the slot is ready) ----
    void le_parameters(uint16_t con_handle, uint16_t interval, uint16_t latency, uint16_t timeout);
    void device_information(uint16_t con_handle, const char* field, const char* value);
    void pnp_id(uint16_t con_handle, uint8_t source, uint16_t vid, uint16_t pid, uint16_t version);
    // Classic link mode (HCI Mode Change): 0 active, 1 hold, 2 sniff, 3 park; interval in 0.625 ms slots.
    void link_mode(uint16_t con_handle, uint8_t mode, uint16_t interval_slots);
    // Whether new controllers are accepted (Bluepad32's flag), and each finished BR/EDR inquiry
    // (the radio really searching, which costs the connected pads air time).
    void searching(bool accepting_new_controllers);
    void inquiry_complete(uint32_t now_ms);
    // Each BLE advertising report: the LE scan is running (it shares the radio with the pads).
    void le_adv_report();

    // ---- Wired USB controllers (USB host), by device address ----
    void usb_mounted(uint8_t address, uint32_t now_ms, uint16_t vid, uint16_t pid, uint16_t bcd_device,
                     uint8_t speed, const char* driver);
    void usb_set_bcd_device(uint8_t address, uint16_t bcd_device);
    void usb_unmounted(uint8_t address);
    void usb_report(uint8_t address, uint32_t now_ms);

    // ---- USB output (to the console / PC) ----
    void usb_output_state(uint32_t now_ms, bool configured, bool suspended);
    void usb_report_sent();

    // Once a second or more often: rates and windows roll over.
    void tick(uint32_t now_ms);

    // JSON report. Returns the length written (always NUL-terminated, truncated if needed).
    size_t report_json(char* out, size_t out_len, uint32_t now_ms);

    // Known 2.4 GHz receivers (they look like wired controllers on USB).
    bool is_wireless_receiver(uint16_t vid, uint16_t pid);

    // Test hook: forget everything.
    void reset_for_tests();

} // namespace diag

#endif // _OGXM_DIAGNOSTICS_H_
