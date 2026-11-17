// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "bootloader.h"
#include <string.h>

#define MD770_BOOT_REQUEST ((volatile uint32_t *)0x20003ffcU)
#define MD770_BOOT_REQUEST_IAP 0x55aafaf5U

void early_hardware_init_pre(void) {
    CKCU->GCCR &= ~CKCU_GCCR_PSRCEN;
    RSTCU->APBPRSTR1 = RSTCU_APBPRSTR1_WDTRST;
    while (RSTCU->APBPRSTR1 & RSTCU_APBPRSTR1_WDTRST) {
    }
    *(volatile uint32_t *)&WDT->MR1 = 0x5fff;
    *(volatile uint32_t *)&WDT->MR0 = 0x1a7d0;
    *(volatile uint32_t *)&WDT->CR  = MD770_WATCHDOG_RELOAD;
    CKCU->GCCR |= CKCU_GCCR_HSIEN;
    while (!(CKCU->GCSR & CKCU_GCSR_HSIRDY)) {
    }
    CKCU->GCCR = (CKCU->GCCR & ~7U) | CKCU_GCCR_SW_HSI;
    while ((CKCU->CKST & 7U) != CKCU_GCCR_SW_HSI) {
    }
    CKCU->AHBCCR &= ~(CKCU_AHBCCR_USBEN | CKCU_AHBCCR_CKREFEN);
    CKCU->GCCR &= ~CKCU_GCCR_PLLEN;
    while (CKCU->GCSR & CKCU_GCSR_PLLRDY) {
    }
    CKCU->PLLCR    = 0;
    CKCU->AHBCFGR  = 0;
    CKCU->APBPCSR0 = 0;
    CKCU->APBPCSR1 = 0;
}

void board_init(void) {
    CKCU->APBCCR0 |= CKCU_APBCCR0_EXTIEN;
    // Preserve the flash chip select and debug pins while selecting GPIO functions.
    palSetGroupMode(GPIOA, 0xcdff, 0, PAL_MODE_HT32_AF(AFIO_GPIO));
    palSetGroupMode(GPIOB, PAL_WHOLE_PORT, 0, PAL_MODE_HT32_AF(AFIO_GPIO));
    palSetGroupMode(GPIOC, PAL_WHOLE_PORT, 0, PAL_MODE_HT32_AF(AFIO_GPIO));
    palSetGroupMode(GPIOD, PAL_WHOLE_PORT, 0, PAL_MODE_HT32_AF(AFIO_GPIO));
    palSetLineMode(A9, PAL_MODE_HT32_AF(AFIO_GPIO) | PAL_HT32_MODE_PU);
    CKCU->HSICR = (CKCU->HSICR & ~0x30U) | 0x23U;
    gpio_write_pin_high(C11);
    gpio_set_pin_output(C11);
}

void matrix_init_custom(void) {
    md770_scan_init();
    md770_link_init();
    md770_power_init();
}

bool matrix_scan_custom(matrix_row_t rows[MATRIX_ROWS]) {
    matrix_row_t previous[MATRIX_ROWS];
    matrix_row_t local[8];
    memcpy(previous, rows, sizeof(previous));
    md770_scan_copy(local);
#ifdef MD770_LEFT
    memcpy(rows + 8, local, sizeof(local));
#else
    memcpy(rows, local, sizeof(local));
    rows[7] = local[7] & 0x0f;
#endif
    if (!md770_power_idle()) md770_link_task(rows);
    bool local_changed = memcmp(previous, rows, 8 * sizeof(matrix_row_t)) != 0;
#ifdef MD770_LEFT
    local_changed = memcmp(previous + 8, rows + 8, 8 * sizeof(matrix_row_t)) != 0;
#endif
    if (memcmp(previous, rows, sizeof(previous)) != 0) {
        md770_power_activity(local_changed);
        md770_power_usb_wake();
    }
    md770_power_task();
    md770_scan_copy(local);
#ifdef MD770_LEFT
    memcpy(rows + 8, local, sizeof(local));
#else
    memcpy(rows, local, sizeof(local));
    rows[7] = local[7] & 0x0f;
#endif
#ifndef MD770_LEFT
    if (md770_power_idle()) {
        memcpy(rows, previous, sizeof(previous));
    }
#endif
    bool changed = memcmp(previous, rows, sizeof(previous)) != 0;
    if (changed) {
#ifdef MD770_LEFT
        local_changed = memcmp(previous + 8, rows + 8, 8 * sizeof(matrix_row_t)) != 0;
#else
        local_changed = memcmp(previous, rows, 8 * sizeof(matrix_row_t)) != 0;
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            if (row == 7) continue;
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                matrix_row_t bit = (matrix_row_t)1 << col;
                if ((previous[row] ^ rows[row]) & bit) md770_radio_matrix_changed((rows[row] & bit) != 0);
            }
        }
#endif
        md770_power_activity(local_changed);
    }
    return changed;
}

static void reset_with_request(uint32_t request) {
#ifndef MD770_LEFT
    md770_radio_shutdown();
#endif
    usbDisconnectBus(&USBD1);
    wait_ms(30);
    chSysDisable();
    *MD770_BOOT_REQUEST = request;
    __DSB();
    NVIC_SystemReset();
}

void bootloader_jump(void) {
    reset_with_request(MD770_BOOT_REQUEST_IAP);
}

void mcu_reset(void) {
    reset_with_request(0);
}

#ifdef MD770_LEFT
void md770_set_layer_led(uint8_t colour) {
    (void)colour;
}
#endif
