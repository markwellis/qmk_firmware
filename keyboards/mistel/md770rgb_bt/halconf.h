// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define HAL_USE_SERIAL TRUE
#define HAL_USE_SPI TRUE
#define HAL_USE_GPT TRUE
#ifndef MD770_LEFT
#    define HAL_USE_PWM TRUE
#endif
#define SERIAL_BUFFERS_SIZE 256
#include_next <halconf.h>
