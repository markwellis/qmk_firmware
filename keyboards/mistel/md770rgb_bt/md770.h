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
void           md770_rgb_task(void);
void           md770_link_init(void);
void           md770_link_task(matrix_row_t rows[MATRIX_ROWS]);
