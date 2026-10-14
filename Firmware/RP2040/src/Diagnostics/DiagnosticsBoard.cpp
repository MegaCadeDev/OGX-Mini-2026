/* Fills in the diagnostics board info at boot (Diagnostics/Diagnostics.h). */
#include <hardware/clocks.h>
#include <hardware/watchdog.h>
#include <pico/time.h>

#include "Diagnostics/Diagnostics.h"
#include "Diagnostics/DiagnosticsBoard.h"
#include "UserSettings/UserSettings.h"

#ifndef OGXM_BOARD_NAME
#define OGXM_BOARD_NAME "unknown"
#endif

namespace diag {

const char* driver_name(DeviceDriverType type)
{
    switch (type) {
        case DeviceDriverType::XBOXOG:    return "XBOXOG";
        case DeviceDriverType::XBOXOG_SB: return "XBOXOG_SB";
        case DeviceDriverType::XBOXOG_XR: return "XBOXOG_XR";
        case DeviceDriverType::XINPUT:    return "XINPUT";
        case DeviceDriverType::PS3:       return "PS3";
        case DeviceDriverType::PS4:       return "PS4";
        case DeviceDriverType::STEAM:     return "STEAM";
        case DeviceDriverType::DINPUT:    return "DINPUT";
        case DeviceDriverType::PSCLASSIC: return "PSCLASSIC";
        case DeviceDriverType::SWITCH:    return "SWITCH";
        case DeviceDriverType::WIIU:      return "WIIU";
        case DeviceDriverType::WII:       return "WII";
        case DeviceDriverType::PS1PS2:    return "PS1PS2";
        case DeviceDriverType::GAMECUBE:  return "GAMECUBE";
        case DeviceDriverType::DREAMCAST: return "DREAMCAST";
        case DeviceDriverType::N64:       return "N64";
        case DeviceDriverType::WEBAPP:    return "WEBAPP";
        default:                          return "OTHER";
    }
}

void board_boot()
{
    /* watchdog_reboot() (mode change, settings saved) leaves scratch 4 clear; a watchdog that
     * fired because something hung leaves the "enabled" marker. */
    const char* reset = watchdog_enable_caused_reboot() ? "watchdog timeout (hang recovery)"
                        : watchdog_caused_reboot()      ? "reboot (mode change / settings saved)"
                                                        : "power-on or reset";
    const DeviceDriverType mode = UserSettings::get_instance().get_current_driver();
#if defined(CONFIG_OGXM_DEBUG)
    const char* build = "Debug";
#else
    const char* build = "Release";
#endif
#if defined(PICO_RP2350)
    const char* chip = "RP2350";
#else
    const char* chip = "RP2040";
#endif
    init(BoardInfo{FIRMWARE_VERSION, OGXM_BOARD_NAME, chip, clock_get_hz(clk_sys) / 1000000, build,
                   driver_name(mode), reset, MAX_GAMEPADS});
    event(to_ms_since_boot(get_absolute_time()), "boot: mode %s, last reset: %s", driver_name(mode), reset);
}

} // namespace diag
