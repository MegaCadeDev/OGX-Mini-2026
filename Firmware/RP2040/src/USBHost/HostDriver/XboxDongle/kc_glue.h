// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal Linux kernel shim so xone's transport/mt76.c builds on the RP2040.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usb_device { uint8_t daddr; };

void kc_log(const char* fmt, ...);
void kc_msleep(uint32_t ms);
int usb_control_msg(struct usb_device* dev, unsigned pipe, uint8_t request, uint8_t requesttype,
                    uint16_t value, uint16_t index, void* data, uint16_t size, int timeout);
int usb_bulk_msg(struct usb_device* dev, unsigned pipe, void* data, int len, int* actual, int timeout);

#ifdef __cplusplus
}
#endif
