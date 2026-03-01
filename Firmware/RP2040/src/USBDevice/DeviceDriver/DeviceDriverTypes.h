#ifndef _DEVICE_DRIVER_TYPES_H_
#define _DEVICE_DRIVER_TYPES_H_

#include <cstdint>

enum class DeviceDriverType : uint8_t
{
    NONE = 0,
    XBOXOG,
    XBOXOG_SB,
    XBOXOG_XR,
    XINPUT,
    PS3,
    DINPUT,
    PSCLASSIC,
    SWITCH,
    WIIU,
    WII,   // Wiimote (Pico W/2W: controller on PIO USB, BT for Wii link)
    WEBAPP = 100,
    UART_BRIDGE
};

#endif // _DEVICE_DRIVER_TYPES_H_