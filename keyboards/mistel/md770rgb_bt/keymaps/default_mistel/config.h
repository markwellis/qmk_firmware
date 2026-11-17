// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define LAYER_STATE_32BIT

#ifndef DYNAMIC_KEYMAP_MACRO_COUNT
#    define DYNAMIC_KEYMAP_MACRO_COUNT 20
#endif

#ifdef RGB_MATRIX_ENABLE
#    define EECONFIG_USER_DATA_SIZE (RGB_MATRIX_LED_COUNT * 3)
#endif
