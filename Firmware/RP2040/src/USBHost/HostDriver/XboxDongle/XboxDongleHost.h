#ifndef _XBOX_DONGLE_HOST_H_
#define _XBOX_DONGLE_HOST_H_

#include <cstdint>

#include "USBHost/HostDriver/XInput/XboxOne.h"

class XboxDongleHost : public XboxOneHost
{
public:
    XboxDongleHost(uint8_t idx)
        : XboxOneHost(idx) {}

    void initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) override;
    void process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len) override;
    bool send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance) override;
    void connect_cb(Gamepad& gamepad, uint8_t address, uint8_t instance) override;
    void disconnect_cb(Gamepad& gamepad, uint8_t address, uint8_t instance) override;

private:
    uint8_t wcid_{0};
    bool rumble_synced_{false};
    uint8_t sent_rumble_l_{0};
    uint8_t sent_rumble_r_{0};
    uint32_t last_send_ms_{0};
};

#endif
