// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "quantum.h"

enum custom_keycodes {
    MD770_NEXT_LAYOUT = QK_KB,
    MD770_RGB_PALETTE,
    MD770_RGB_RED,
    MD770_RGB_GREEN,
    MD770_RGB_BLUE,
    MD770_RGB_NEXT,
    MD770_PROFILE0,
    MD770_PROFILE1,
    MD770_PROFILE2,
    MD770_PROFILE3,
    MD770_RESET_PROFILE,
    MD770_RGB_EDIT,
    MD770_RGB_CUSTOM,
};

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
void           md770_set_layer_led(uint8_t colour);
