// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"

#define MD770_RGB_SLOTS 96
#define MD770_LEFT_RGB_SLOTS 42

void           md770_scan_init(void);
void           md770_scan_copy(matrix_row_t rows[8]);
void           md770_rgb_init(void);
void           md770_rgb_latch(void);
void           md770_rgb_update(void);
void           md770_rgb_receive(const uint8_t *payload);
const uint8_t *md770_rgb_left(void);
void           md770_link_init(void);
void           md770_link_task(matrix_row_t rows[MATRIX_ROWS]);
bool           md770_link_can_sleep(void);
void           md770_link_sleep(bool sleeping);
bool           md770_link_connected(void);
bool           md770_link_rgb_active(void);
void           md770_set_layer_led(uint8_t colour);
void           md770_scan_set_idle(bool idle);
void           md770_scan_pause(void);
void           md770_scan_resume(void);
void           md770_scan_idle_begin(void);
bool           md770_scan_idle_changed(bool *held);
void           md770_scan_prepare_wake(void);
void           md770_scan_finish_wake(void);
void           md770_scan_idle_normalise(void);
void           md770_rgb_sleep(bool asleep);
void           md770_power_init(void);
void           md770_power_task(void);
void           md770_power_service(void);
bool           md770_power_rgb_allowed(void);
bool           md770_power_usb_suspended(void);
bool           md770_power_sleeping(void);
void           md770_power_tick(void);
void           md770_power_keep_awake(uint32_t minimum);
void           md770_power_limit_awake(uint32_t maximum);
void           md770_power_activity(bool local);
void           md770_power_radio_activity(void);
bool           md770_power_idle(void);
void           md770_power_usb_wake(void);
bool           md770_power_usb_ready(void);
bool           md770_battery_low(void);
bool           md770_charge_indicated(void);
#ifndef MD770_LEFT
void    md770_playback_cancel_physical(void);
void    md770_radio_discard_reports(void);
void    md770_radio_release_reports(void);
void    md770_radio_sync_reports(void);
void    md770_indicators_task(void);
void    md770_indicators_sleep(bool asleep);
bool    md770_storage_sleep(void);
bool    md770_storage_wake(void);
void    md770_radio_led_update(void);
void    md770_radio_indicate(bool pair);
void    md770_radio_select(uint8_t slot, bool pair);
void    md770_radio_shutdown(void);
bool    md770_radio_key_ready(uint16_t keycode);
void    md770_radio_tick(void);
void    md770_radio_power_downI(void);
void    md770_radio_power_upI(void);
uint8_t md770_radio_state(void);
void    md770_radio_cancel_indication(void);
void    md770_radio_release_keep_awake(void);
void    md770_radio_matrix_changed(bool pressed);
bool    md770_radio_pairing_chord(void);
bool    md770_radio_usb_allowed(void);
#endif
