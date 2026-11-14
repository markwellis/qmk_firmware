// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include <string.h>

#ifdef MD770_LEFT
static const pin_t select_pins[8] = {B10, B11, D0, A10, A11, A8, D1, D2};
#    define SENSE_MASK 0x00ffU
#    define SENSE_SHIFT 0
#else
static const pin_t select_pins[8] = {B14, B7, D0, A15, A11, B13, D2, C1};
#    define SENSE_MASK 0x01feU
#    define SENSE_SHIFT 1
#endif
static volatile matrix_row_t scanned[8];

static void scan_tick(GPTDriver *driver) {
    (void)driver;
    // Matrix inputs share GPIOA with the strobed LED rows.
    palClearPort(GPIOA, SENSE_MASK);
    GPTM1->CTR  = 0;
    GPTM1->CNTR = 0;
    md770_rgb_latch();

    GPIOA->INER |= SENSE_MASK;
    for (uint8_t row = 0; row < 8; ++row) {
        GPIOA->DIRCR &= ~SENSE_MASK;
        gpio_write_pin_high(select_pins[row]);
        // The scan IRQ cannot reuse the foreground delay timer.
        __NOP();
        __NOP();
        __NOP();
        scanned[row] = (palReadPort(GPIOA) >> SENSE_SHIFT) & 0xff;
        gpio_write_pin_low(select_pins[row]);
        GPIOA->DIRCR |= SENSE_MASK;
    }
    GPIOA->INER &= ~SENSE_MASK;
    md770_rgb_update();
}

void md770_scan_init(void) {
    for (uint8_t i = 0; i < 8; ++i) {
        gpio_write_pin_low(select_pins[i]);
        gpio_set_pin_output(select_pins[i]);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        gpio_set_pin_input_low(PAL_LINE(GPIOA, i + SENSE_SHIFT));
    }
    md770_rgb_init();
    static const GPTConfig scan_config = {.frequency = 48000000, .callback = scan_tick};
    gptStart(&GPTD_BFTM0, &scan_config);
    scan_tick(NULL);
    gptStartContinuous(&GPTD_BFTM0, 39441);
}

void md770_scan_copy(matrix_row_t rows[8]) {
    chSysLock();
    for (uint8_t i = 0; i < 8; ++i) {
        rows[i] = scanned[i];
    }
    chSysUnlock();
}
