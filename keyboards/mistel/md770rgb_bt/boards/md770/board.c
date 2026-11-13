// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "hal.h"
#include "gpio.h"

const PALConfig pal_default_config = {
    .setup =
        {
            {.PU = 1U << 9, .CFG = {0x11111111, 0x11001111}},
            {.CFG = {0x11111111, 0x11111111}},
            {.CFG = {0x11111111, 0x11111111}},
            {.CFG = {0x11111111, 0x11111111}},
        },
};

void __early_init(void) {
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
    ht32_clock_init();
}

void boardInit(void) {
    CKCU->HSICR = (CKCU->HSICR & ~0x30U) | 0x23U;
    gpio_write_pin_high(C11);
    gpio_set_pin_output(C11);
}
