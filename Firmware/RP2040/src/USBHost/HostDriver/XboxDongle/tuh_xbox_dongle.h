#ifndef _TUH_XBOX_DONGLE_H_
#define _TUH_XBOX_DONGLE_H_

#include <cstdint>

#include "tusb.h"
#include "host/usbh_pvt.h"

namespace tuh_xbox_dongle {

const usbh_class_driver_t* class_driver();
bool is_supported(uint16_t vid, uint16_t pid);
void task();
uint8_t active_wcid();
bool send_rumble(uint8_t wcid, uint8_t rumble_l, uint8_t rumble_r);

}

#endif
