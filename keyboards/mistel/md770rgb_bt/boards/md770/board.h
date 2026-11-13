// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define BOARD_NAME "Mistel MD770 RGB BT"
#define HT32F52352
#define FLASH_SIZE (0x1fe00 - 0x3200)

#if !defined(_FROM_ASM_)
#    ifdef __cplusplus
extern "C" {
#    endif
void boardInit(void);
#    ifdef __cplusplus
}
#    endif
#endif
