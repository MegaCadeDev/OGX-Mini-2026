#include <cstring>

#include "host/usbh.h"
#include "Board/board_api.h"

#include "USBHost/HostDriver/XInput/tuh_xinput/tuh_xinput.h"
#include "USBHost/HostDriver/XInput/XboxOne.h"

static constexpr uint32_t GUIDE_STALE_MS = 400u;  // clear Guide if no 0x07 packet this long (stuck/noise)

void XboxOneHost::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len)
{
    tuh_xinput::receive_report(address, instance);
}

void XboxOneHost::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len)
{
    const uint8_t cmd = report[0];
    if (cmd == XboxOne::GIP_CMD_VIRTUAL_KEY)
    {
        last_guide_07_ms_ = board_api::ms_since_boot();
        // Guide (Home) in 0x07: payload byte 4 = pressed (bit 0) / released; or keycode 0x5B at 4, pressed at 5
        if (len >= 5) {
            if (report[4] == 0x5B && len >= 6)
                guide_pressed_ = (report[5] & 0x01) ? 1 : 0;  // keycode then pressed
            else
                guide_pressed_ = (report[4] & 0x01) ? 1 : 0;   // pressed bit in first payload byte
        }
        // Push state immediately so Guide registers (next 0x20 might be unchanged and skip set_pad_in)
        const XboxOne::InReport* prev = &prev_in_report_;
        Gamepad::PadIn gp_in;
        const uint16_t b = prev->buttons;
        if (b & XboxOne::GipWireButtons::DPAD_UP)    gp_in.dpad |= gamepad.MAP_DPAD_UP;
        if (b & XboxOne::GipWireButtons::DPAD_DOWN)  gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
        if (b & XboxOne::GipWireButtons::DPAD_LEFT)  gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
        if (b & XboxOne::GipWireButtons::DPAD_RIGHT) gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;
        if (b & XboxOne::GipWireButtons::LEFT_THUMB)  gp_in.buttons |= gamepad.MAP_BUTTON_L3;
        if (b & XboxOne::GipWireButtons::RIGHT_THUMB) gp_in.buttons |= gamepad.MAP_BUTTON_R3;
        if (b & XboxOne::GipWireButtons::LEFT_SHOULDER)  gp_in.buttons |= gamepad.MAP_BUTTON_LB;
        if (b & XboxOne::GipWireButtons::RIGHT_SHOULDER) gp_in.buttons |= gamepad.MAP_BUTTON_RB;
        if (b & XboxOne::GipWireButtons::BACK)  gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
        if (b & XboxOne::GipWireButtons::START) gp_in.buttons |= gamepad.MAP_BUTTON_START;
        if (b & XboxOne::GipWireButtons::SYNC)  gp_in.buttons |= gamepad.MAP_BUTTON_MISC;
        if (guide_pressed_) gp_in.buttons |= gamepad.MAP_BUTTON_SYS;
        if (b & XboxOne::GipWireButtons::A)     gp_in.buttons |= gamepad.MAP_BUTTON_A;
        if (b & XboxOne::GipWireButtons::B)     gp_in.buttons |= gamepad.MAP_BUTTON_B;
        if (b & XboxOne::GipWireButtons::X)     gp_in.buttons |= gamepad.MAP_BUTTON_X;
        if (b & XboxOne::GipWireButtons::Y)     gp_in.buttons |= gamepad.MAP_BUTTON_Y;
        gp_in.trigger_l = gamepad.scale_trigger_l(static_cast<uint8_t>(prev->trigger_l >> 2));
        gp_in.trigger_r = gamepad.scale_trigger_r(static_cast<uint8_t>(prev->trigger_r >> 2));
        std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad.scale_joystick_l(prev->joystick_lx, prev->joystick_ly, true);
        std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad.scale_joystick_r(prev->joystick_rx, prev->joystick_ry, true);
        gamepad.set_pad_in(gp_in);
        tuh_xinput::receive_report(address, instance);
        return;
    }

    if (cmd != XboxOne::GIP_CMD_INPUT)
    {
        tuh_xinput::receive_report(address, instance);
        return;
    }

    const XboxOne::InReport* in_report = reinterpret_cast<const XboxOne::InReport*>(report);
    // Clear Guide if we haven't seen a 0x07 packet in a while (stuck / noise)
    bool guide_cleared = false;
    if (guide_pressed_ && (board_api::ms_since_boot() - last_guide_07_ms_) > GUIDE_STALE_MS) {
        guide_pressed_ = 0;
        guide_cleared = true;
    }
    if (std::memcmp(&prev_in_report_ + 4, in_report + 4, 14) == 0 && !guide_cleared)
    {
        tuh_xinput::receive_report(address, instance);
        return;
    }

    Gamepad::PadIn gp_in;

    // Raw GIP 0x20 uses Microsoft bit layout; map to project Gamepad (then WiiReportConverter → Wii)
    const uint16_t b = in_report->buttons;
    if (b & XboxOne::GipWireButtons::DPAD_UP)    gp_in.dpad |= gamepad.MAP_DPAD_UP;
    if (b & XboxOne::GipWireButtons::DPAD_DOWN)  gp_in.dpad |= gamepad.MAP_DPAD_DOWN;
    if (b & XboxOne::GipWireButtons::DPAD_LEFT)  gp_in.dpad |= gamepad.MAP_DPAD_LEFT;
    if (b & XboxOne::GipWireButtons::DPAD_RIGHT) gp_in.dpad |= gamepad.MAP_DPAD_RIGHT;

    if (b & XboxOne::GipWireButtons::LEFT_THUMB)  gp_in.buttons |= gamepad.MAP_BUTTON_L3;
    if (b & XboxOne::GipWireButtons::RIGHT_THUMB) gp_in.buttons |= gamepad.MAP_BUTTON_R3;
    if (b & XboxOne::GipWireButtons::LEFT_SHOULDER)  gp_in.buttons |= gamepad.MAP_BUTTON_LB;
    if (b & XboxOne::GipWireButtons::RIGHT_SHOULDER) gp_in.buttons |= gamepad.MAP_BUTTON_RB;
    if (b & XboxOne::GipWireButtons::BACK)  gp_in.buttons |= gamepad.MAP_BUTTON_BACK;
    if (b & XboxOne::GipWireButtons::START) gp_in.buttons |= gamepad.MAP_BUTTON_START;
    if (b & XboxOne::GipWireButtons::SYNC)  gp_in.buttons |= gamepad.MAP_BUTTON_MISC;
    if (guide_pressed_) gp_in.buttons |= gamepad.MAP_BUTTON_SYS;
    if (b & XboxOne::GipWireButtons::A)     gp_in.buttons |= gamepad.MAP_BUTTON_A;
    if (b & XboxOne::GipWireButtons::B)     gp_in.buttons |= gamepad.MAP_BUTTON_B;
    if (b & XboxOne::GipWireButtons::X)     gp_in.buttons |= gamepad.MAP_BUTTON_X;
    if (b & XboxOne::GipWireButtons::Y)     gp_in.buttons |= gamepad.MAP_BUTTON_Y;

    gp_in.trigger_l = gamepad.scale_trigger_l(static_cast<uint8_t>(in_report->trigger_l >> 2));
    gp_in.trigger_r = gamepad.scale_trigger_r(static_cast<uint8_t>(in_report->trigger_r >> 2));

    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad.scale_joystick_l(in_report->joystick_lx, in_report->joystick_ly, true);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad.scale_joystick_r(in_report->joystick_rx, in_report->joystick_ry, true);

    gamepad.set_pad_in(gp_in);

    tuh_xinput::receive_report(address, instance);
    std::memcpy(&prev_in_report_, in_report, 18);
}

bool XboxOneHost::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    Gamepad::PadOut gp_out = gamepad.get_pad_out();
    return tuh_xinput::set_rumble(address, instance, gp_out.rumble_l, gp_out.rumble_r, false);
}