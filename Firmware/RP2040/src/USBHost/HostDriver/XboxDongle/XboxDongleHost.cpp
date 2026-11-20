#include "USBHost/HostDriver/XboxDongle/XboxDongleHost.h"

#include "Board/board_api.h"
#include "USBHost/HostDriver/XboxDongle/tuh_xbox_dongle.h"

namespace {
constexpr uint32_t RUMBLE_REFRESH_MS = 500;
}

void XboxDongleHost::initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len)
{
    (void)address;
    (void)instance;
    (void)report_desc;
    (void)desc_len;
    wcid_ = 0;
    rumble_synced_ = false;
    gamepad.set_pad_in(Gamepad::PadIn());
}

void XboxDongleHost::process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len)
{
    XboxOneHost::process_report(gamepad, address, instance, report, len);
}

void XboxDongleHost::connect_cb(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    (void)gamepad;
    (void)address;
    (void)instance;
    wcid_ = tuh_xbox_dongle::active_wcid();
    rumble_synced_ = false;
}

void XboxDongleHost::disconnect_cb(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    (void)address;
    (void)instance;
    wcid_ = 0;
    rumble_synced_ = false;
    gamepad.set_pad_in(Gamepad::PadIn());
}

bool XboxDongleHost::send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance)
{
    (void)address;
    (void)instance;
    if (!wcid_)
    {
        rumble_synced_ = false;
        return false;
    }
    const Gamepad::PadOut gp_out = gamepad.get_pad_out();
    const uint32_t now = board_api::ms_since_boot();
    const bool changed = !rumble_synced_ || gp_out.rumble_l != sent_rumble_l_ || gp_out.rumble_r != sent_rumble_r_;
    const bool active = gp_out.rumble_l || gp_out.rumble_r;
    if (!changed && !(active && now - last_send_ms_ >= RUMBLE_REFRESH_MS))
    {
        return true;
    }
    if (!tuh_xbox_dongle::send_rumble(wcid_, gp_out.rumble_l, gp_out.rumble_r))
    {
        rumble_synced_ = false;
        return false;
    }
    last_send_ms_ = now;
    sent_rumble_l_ = gp_out.rumble_l;
    sent_rumble_r_ = gp_out.rumble_r;
    rumble_synced_ = true;
    return true;
}
