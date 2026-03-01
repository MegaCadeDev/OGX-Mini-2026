#include "Board/Config.h"
#include "OGXMini/Board/PicoW.h"
#if (OGXM_BOARD == PI_PICOW)

#include <cstdio>
#include <string>
#include <hardware/clocks.h>
#include <pico/multicore.h>
#include <pico/time.h>

#include "tusb.h"
#include "bsp/board_api.h"

#include "USBDevice/DeviceManager.h"
#include "USBDevice/DeviceDriver/DeviceDriverTypes.h"
#include "UserSettings/UserSettings.h"
#include "Board/board_api.h"
#include "Board/ogxm_log.h"
#include "Bluepad32/Bluepad32.h"
#include "BLEServer/BLEServer.h"
#include "Gamepad/Gamepad.h"
#include "TaskQueue/TaskQueue.h"

#if defined(CONFIG_EN_USB_HOST)
#include "pio_usb.h"
#include "USBHost/HostManager.h"
#include <pico/flash.h>
#include "Wii/WiiReportConverter.h"

/** Called from flash_safe_execute on Core1; only write flash. Reboot is done by caller after return. */
static void store_driver_type_callback(void* arg) {
    auto d = *static_cast<DeviceDriverType*>(arg);
    UserSettings::get_instance().store_driver_type_only(d);
}

extern "C" {
#include "Wii/wiimote-lib/wiimote.h"
#include "Wii/wiimote-lib/wiimote_btstack.h"
}

static WiimoteReport s_wiimote_report = {};  // shared: Core0 reads, Core1 writes

extern "C" void wii_led_on(void)  { board_api::set_led(true); }
extern "C" void wii_led_off(void) { board_api::set_led(false); }
#endif

Gamepad _gamepads[MAX_GAMEPADS];

#if defined(CONFIG_EN_USB_HOST)
constexpr uint32_t FEEDBACK_DELAY_MS = 200;

void core1_task_wii_usb_host() {
    OGXM_LOG("WII Core1: entry\n");
    flash_safe_execute_core_init();  // so Core0 can flash_safe_execute when saving Wii address (TLV)
    HostManager& host_manager = HostManager::get_instance();
    host_manager.initialize(_gamepads);
    OGXM_LOG("WII Core1: HostManager init done\n");

    OGXM_LOG("WII Core1: waiting for host_connected() (plug USB controller into external port GP0/GP1)\n");
    uint32_t wait_count = 0;
    while (!board_api::usb::host_connected()) {
        sleep_ms(100);
        if ((++wait_count % 20) == 0) {
            OGXM_LOG("WII Core1: still waiting for host_connected()...\n");
        }
    }
    OGXM_LOG("WII Core1: host_connected\n");

    // Use an unused hardware alarm for PIO USB host so TaskQueue Core1 can keep alarm 2 (avoids conflict; alarm 3 may be claimed on Pico 2 W)
    static alarm_pool_t* s_wii_usb_alarm_pool = alarm_pool_create_with_unused_hardware_alarm(1);
    pio_usb_configuration_t pio_cfg = PIO_USB_CONFIG;
    pio_cfg.alarm_pool = s_wii_usb_alarm_pool;
    // CYW43 (BT on Core0) claims DMA 0 and 1; use channel 2 for PIO USB TX to avoid "DMA channel 0 is already claimed"
    pio_cfg.tx_ch = 2;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    OGXM_LOG("WII Core1: tuh_configure done\n");
    tuh_init(BOARD_TUH_RHPORT);
    OGXM_LOG("WII Core1: tuh_init done\n");

    uint32_t tid_feedback = TaskQueue::Core1::get_new_task_id();
    TaskQueue::Core1::queue_delayed_task(tid_feedback, FEEDBACK_DELAY_MS, true,
        [&host_manager]() { host_manager.send_feedback(); });
    OGXM_LOG("WII Core1: feedback task queued, entering loop\n");

    s_wiimote_report.mode = (uint8_t)NO_EXTENSION;  // Wiimote only; home+down cycles to Nunchuk, then Classic

#if !defined(CONFIG_OGXM_FIXED_DRIVER) || defined(CONFIG_OGXM_FIXED_DRIVER_ALLOW_COMBOS)
    uint32_t last_gp_check_ms = 0;  // throttle: same 600 ms / 3 s hold as other modes
    uint32_t gp_check_ticks = 0;
#endif

    while (true) {
        TaskQueue::Core1::process_tasks();
        tuh_task();
        Wii::gamepad_to_wiimote_report(_gamepads[0], &s_wiimote_report);

#if !defined(CONFIG_OGXM_FIXED_DRIVER) || defined(CONFIG_OGXM_FIXED_DRIVER_ALLOW_COMBOS)
        uint32_t now = board_api::ms_since_boot();
        if (now - last_gp_check_ms >= static_cast<uint32_t>(UserSettings::GP_CHECK_DELAY_MS)) {
            last_gp_check_ms = now;
            UserSettings& user_settings = UserSettings::get_instance();
            bool changed = user_settings.check_for_driver_change(_gamepads[0]);
#if defined(CONFIG_OGXM_DEBUG)
            if ((++gp_check_ticks % 10) == 0)
                OGXM_LOG("WII Core1: gp check tick " + std::to_string(gp_check_ticks) + "\n");
#endif
            if (changed) {
                static DeviceDriverType new_driver;
                new_driver = user_settings.get_current_driver();
                OGXM_LOG("WII Core1: driver change detected, new_driver=" + OGXM_TO_STRING(new_driver) + ", calling flash_safe_execute\n");
                int fs_ret = flash_safe_execute(store_driver_type_callback, &new_driver, 200);
                OGXM_LOG("WII Core1: flash_safe_execute returned " + std::to_string(fs_ret) + " (0=ok)\n");
                if (fs_ret == 0) {
                    OGXM_LOG("WII Core1: calling reboot\n");
                    board_api::reboot();
                }
            }
        }
#endif
    }
}
#endif

void core1_task() {
    board_api::init_bluetooth();
    board_api::set_led(true);
    BLEServer::init_server(_gamepads);
    bluepad32::run_task(_gamepads);
}

void set_gp_check_timer(uint32_t task_id) {
#if !defined(CONFIG_OGXM_FIXED_DRIVER) || defined(CONFIG_OGXM_FIXED_DRIVER_ALLOW_COMBOS)
    UserSettings& user_settings = UserSettings::get_instance();
    TaskQueue::Core0::queue_delayed_task(task_id, UserSettings::GP_CHECK_DELAY_MS, true,
    [&user_settings] {
        //Check gamepad inputs for button combo to change usb device driver
        if (user_settings.check_for_driver_change(_gamepads[0])) {
            //This will store the new mode and reboot the pico
            user_settings.store_driver_type(user_settings.get_current_driver());
        }
    });
#else
    (void)task_id;  // Fixed output, combos disabled
#endif
}

void pico_w::initialize() {
    OGXM_LOG("PicoW init: start\n");
    board_api::init_board();
    OGXM_LOG("PicoW init: board inited\n");

    UserSettings& user_settings = UserSettings::get_instance();
    user_settings.initialize_flash();
#if defined(CONFIG_OGXM_DEBUG)
    OGXM_LOG("PicoW init: flash inited, driver=" + OGXM_TO_STRING(user_settings.get_current_driver()) + "\n");
#else
    OGXM_LOG("PicoW init: flash inited\n");
#endif

    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
        _gamepads[i].set_profile(user_settings.get_profile_by_index(i));
    }

    DeviceManager& device_manager = DeviceManager::get_instance();
    device_manager.initialize_driver(user_settings.get_current_driver(), _gamepads);
    OGXM_LOG("PicoW init: driver inited\n");
}

void pico_w::run() {
    OGXM_LOG("PicoW run: start\n");
    multicore_reset_core1();

    UserSettings& user_settings = UserSettings::get_instance();
    DeviceDriver* device_driver = DeviceManager::get_instance().get_driver();
    const bool wii_mode = (user_settings.get_current_driver() == DeviceDriverType::WII);
    OGXM_LOG("PicoW run: wii_mode=" + std::string(wii_mode ? "1" : "0") + "\n");

#if defined(CONFIG_EN_USB_HOST)
    if (wii_mode) {
        OGXM_LOG("PicoW run: launching Core1 (Wii USB host)\n");
        multicore_launch_core1(core1_task_wii_usb_host);
        OGXM_LOG("PicoW run: Core1 launched, starting Wiimote BT (discoverable as Nintendo RVL-CNT-01)\n");
        board_api::init_bluetooth();
        wiimote_emulator_set_led(wii_led_on, wii_led_off);
        wiimote_emulator(&s_wiimote_report);  // never returns: BT run loop, LED blinks until connected
    } else
#endif
    {
        tud_init(BOARD_TUD_RHPORT);
        OGXM_LOG("PicoW run: tud_init done, launching Core1 (BT)\n");
        multicore_launch_core1(core1_task);
        OGXM_LOG("PicoW run: Core1 (BT) launched\n");
    }

    uint32_t tid_gp_check = TaskQueue::Core0::get_new_task_id();
    set_gp_check_timer(tid_gp_check);
    OGXM_LOG("PicoW run: gp check timer set, entering main loop\n");

    static bool mounted_logged = false;
    static uint32_t loop_count = 0;
    while (true) {
        TaskQueue::Core0::process_tasks();

        for (uint8_t i = 0; i < MAX_GAMEPADS; ++i) {
            device_driver->process(i, _gamepads[i]);
        }
        if (!wii_mode) {
            tud_task();
            if (tud_mounted() && !mounted_logged) {
                mounted_logged = true;
                OGXM_LOG("PicoW run: USB configured\n");
            }
        } else if (loop_count == 0 || loop_count == 5000) {
            OGXM_LOG("PicoW run: Wii main loop tick\n");
        }
        loop_count++;
#if MAIN_LOOP_DELAY_US > 0
        sleep_us(MAIN_LOOP_DELAY_US);
#endif
    }
}

// #else // (OGXM_BOARD == PI_PICOW)

// void pico_w::initialize() {}
// void pico_w::run() {}

#endif // (OGXM_BOARD == PI_PICOW)
