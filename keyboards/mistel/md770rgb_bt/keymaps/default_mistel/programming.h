// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include QMK_KEYBOARD_H
#include "keymap.h"

#ifdef DYNAMIC_KEYMAP_ENABLE
bool          md770_programming_process(uint16_t keycode, keyrecord_t *record);
void          md770_programming_task(void);
void          md770_playback_cancel(void);
bool          md770_playback_active(void);
uint8_t       md770_programming_indicator(uint8_t colour);
bool          md770_programming_blocks_fn_swap(layer_state_t state);
layer_state_t md770_programming_layers(layer_state_t state, md770_profile_t profile);
#else
static inline bool md770_programming_process(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}
static inline void md770_programming_task(void) {}
static inline void md770_playback_cancel(void) {}
static inline bool md770_playback_active(void) {
    return false;
}
static inline uint8_t md770_programming_indicator(uint8_t colour) {
    return colour;
}
static inline bool md770_programming_blocks_fn_swap(layer_state_t state) {
    (void)state;
    return false;
}
static inline layer_state_t md770_programming_layers(layer_state_t state, md770_profile_t profile) {
    (void)profile;
    return state;
}
#endif
