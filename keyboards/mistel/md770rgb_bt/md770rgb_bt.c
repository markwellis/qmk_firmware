// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "bootloader.h"
#include <string.h>

#define MD770_BOOT_REQUEST ((volatile uint32_t *)0x20003ffcU)
#define MD770_BOOT_REQUEST_IAP 0x55aafaf5U

void early_hardware_init_pre(void) {
    WDT->CR = MD770_WATCHDOG_RELOAD;
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
    md770_link_task(rows);
    return memcmp(previous, rows, sizeof(previous)) != 0;
}

static void reset_with_request(uint32_t request) {
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
