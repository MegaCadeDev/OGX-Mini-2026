#ifndef _OGXM_DIAGNOSTICS_H_
#define _OGXM_DIAGNOSTICS_H_

#include <cstddef>
#include <cstdint>

/*  Diagnostics: what a user can send without a serial adapter.
 *
 *  Kept in RAM (no flash writes while playing). For each Bluetooth controller: what it is (name,
 *  IDs, address prefix, BLE Device Information strings), its link (Classic or LE, LE interval /
 *  latency / timeout, link mode) and its input
 *  timing (reports per second, typical interval, late reports, largest gap, reports lost by the
 *  controller's own counter). The same timing for wired USB controllers, the USB output side, and
 *  a ring of recent events. The web app asks for it in Web App mode (GET_DIAGNOSTICS) and saves
 *  a report.
 *
 *  Measurements of the mode actually used for playing would be lost by the reboot into Web App
 *  mode, so a short summary of the session is kept in RAM across reboots and, at the moments
 *  described with session_freeze() below, in flash.
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
    constexpr size_t kSessionBytes = 240;  // one flash entry (NVSTool value size)
    constexpr size_t kSessionEvents = 4;   // last events kept with the session summary
    constexpr size_t kCrashBytes = 96;

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

    /* Kept across a reboot that keeps the RAM (mode change, settings saved, watchdog, crash; not
     * a power-on): the events ring goes on from the previous boot, each event tagged with its boot
     * (0 = this one, -1 = the one before...), and a crash record. Nothing is written to flash for
     * this. */
    void init(const BoardInfo& info);

    // ---- Crashes (Diagnostics/CrashHandler.cpp: hard fault, panic) ----
    struct CrashInfo {
        uint8_t kind;          // 1 = hard fault, 2 = panic
        uint8_t core;
        uint8_t has_fault_regs;  // RP2350 (Cortex-M33) only
        uint8_t reserved;
        uint32_t pc, lr, xpsr;
        uint32_t cfsr, hfsr, mmfar, bfar;
        uint32_t uptime_ms;
        char message[48];      // panic message
    };
    // From the fault / panic handler, before the reboot: no lock, no allocation.
    void crash_record(const CrashInfo& crash);
    // At boot: a crash recorded during the previous boot, as bytes for flash (kCrashBytes); the
    // caller stores it if it differs from the stored one. 0 when there is none.
    size_t new_crash(uint8_t* out, size_t out_len);
    // At boot, when there is no new one: the crash stored in flash, for the report.
    void set_stored_crash(const uint8_t* data, size_t len);

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
    /* Input-to-use latency of the output driver (Gamepad::latency_stats()), for the session
     * summary. The board registers where to read it; it is read when the session ends, outside
     * the diagnostics lock (it takes the gamepad's mutex). */
    struct Latency { uint32_t samples, avg_us, max_us; };
    using LatencySource = Latency (*)();
    void set_latency_source(LatencySource source);

    // Once a second or more often: rates and windows roll over.
    void tick(uint32_t now_ms);

    // ---- Session summary kept across the mode-change reboot ----
    /* Sessions (a summary of a mode's use: rates, losses, last events). A session is kept when it
     * is relevant (not Web App mode; a controller was in it or it lasted 30 s or more).
     *  - session_freeze(): when the session ends (a mode change, the last controller going
     *    away), before the pads are turned off. The latest relevant session is kept in RAM
     *    across reboots ("previous_session", no flash write) until a newer one replaces it.
     *  - take_replaced_session(): the RAM session that the new one just replaced, if it was not
     *    stored yet; the caller writes it to flash (a mode change).
     *  - session_capture(): the frozen session (0 if not relevant); the caller writes it to
     *    flash when the last controller disconnected, then mark_session_stored().
     *  - set_stored_session(): the one in flash, at boot ("stored_session"). */
    void session_freeze(uint32_t now_ms);
    size_t session_capture(uint8_t* out, size_t out_len, uint32_t now_ms);
    size_t take_replaced_session(uint8_t* out, size_t out_len);
    void mark_session_stored();
    void set_stored_session(const uint8_t* data, size_t len);

    // JSON report. Returns the length written (always NUL-terminated, truncated if needed).
    size_t report_json(char* out, size_t out_len, uint32_t now_ms);

    // Known 2.4 GHz receivers (they look like wired controllers on USB).
    bool is_wireless_receiver(uint16_t vid, uint16_t pid);

    // Test hook: forget everything.
    void reset_for_tests();

} // namespace diag

#endif // _OGXM_DIAGNOSTICS_H_
