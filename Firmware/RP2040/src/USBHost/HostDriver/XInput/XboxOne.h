#ifndef _XBOX_ONE_HOST_H_
#define _XBOX_ONE_HOST_H_

#include <cstdint>

#include "Descriptors/XboxOne.h"
#include "USBHost/HostDriver/HostDriver.h"

class XboxOneHost : public HostDriver
{
public:
    XboxOneHost(uint8_t idx)
        : HostDriver(idx) {}

    void initialize(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report_desc, uint16_t desc_len) override;
    void process_report(Gamepad& gamepad, uint8_t address, uint8_t instance, const uint8_t* report, uint16_t len) override;
    bool send_feedback(Gamepad& gamepad, uint8_t address, uint8_t instance) override;

private:
    XboxOne::InReport prev_in_report_;
    uint8_t guide_pressed_{0};       // Guide (Home) from GIP 0x07; not in 0x20 report
    uint32_t last_guide_07_ms_{0};   // when we last saw 0x07; clear Guide if no 0x07 for a while
};

#endif // _XBOX_ONE_HOST_H_