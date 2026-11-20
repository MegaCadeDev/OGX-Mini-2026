// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Xbox Wireless Adapter dongle logic, adapted from xone (transport/dongle.c).
 * Copyright (C) 2021 Severin von Wnuck-Lipinski <severinvonw@outlook.de>
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DONGLE_CORE_MAX_CLIENTS 8

bool dongle_core_start(uint8_t daddr);
void dongle_core_stop(void);
void dongle_core_task(void);
bool dongle_core_ready(void);
void dongle_core_request_pairing(void);
int dongle_core_send_gip(uint8_t wcid, const uint8_t* pkt, uint32_t len);
uint8_t dongle_core_next_sequence(void);

void dongle_core_on_gip(uint8_t wcid, const uint8_t* data, uint32_t len);
void dongle_core_on_client(uint8_t wcid, bool connected);

#ifdef __cplusplus
}
#endif
