// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770.h"
#include "bootloader.h"
#include <string.h>

void matrix_init_custom(void) {
    md770_scan_init();
    md770_link_init();
}

bool matrix_scan_custom(matrix_row_t rows[MATRIX_ROWS]) {
    matrix_row_t previous[MATRIX_ROWS];
    matrix_row_t local[8];
    memcpy(previous, rows, sizeof(previous));
    md770_scan_copy(local);
#ifdef MD770_LEFT
    memcpy(rows + 7, local, sizeof(local));
#else
    memcpy(rows, local, 7 * sizeof(matrix_row_t));
#endif
    md770_rgb_task();
    md770_link_task(rows);
    return memcmp(previous, rows, sizeof(previous)) != 0;
}

void bootloader_jump(void) {
    usbDisconnectBus(&USBD1);
    wait_ms(30);
    chSysDisable();
    *(volatile uint32_t *)0x20003ffc = 0x55aafaf5;
    __DSB();
    NVIC_SystemReset();
}
