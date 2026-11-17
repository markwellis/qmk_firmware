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
static uint8_t select_function(uint8_t row) {
#ifdef MD770_LEFT
    return row < 2;
#else
    return row == 0 || row == 5;
#endif
}

static volatile matrix_row_t scanned[8];
static volatile bool         idle;
static const uint32_t        scan_interval = 39441;
static uint8_t               debounce_count[8], samples[8], stable_rows[8], last_rows[8], idle_rows[8];

static void scan_rows(void) {
    GPIOA->INER |= SENSE_MASK;
    for (uint8_t row = 0; row < 8; ++row) {
        GPIOA->DIRCR &= ~SENSE_MASK;
        gpio_write_pin_high(select_pins[row]);
        __NOP();
        __NOP();
        __NOP();
        uint8_t sample = (palReadPort(GPIOA) >> SENSE_SHIFT) & 0xff;
        scanned[row]   = sample;
        if (sample != samples[row]) {
            samples[row]        = sample;
            debounce_count[row] = 8;
        } else if (debounce_count[row]) {
            if (debounce_count[row] == 1) stable_rows[row] = sample;
            --debounce_count[row];
        }
        gpio_write_pin_low(select_pins[row]);
        GPIOA->DIRCR |= SENSE_MASK;
    }
}

static void scan_tick(GPTDriver *driver) {
    (void)driver;
    palClearPort(GPIOA, SENSE_MASK);
    GPTM1->CTR  = 0;
    GPTM1->CNTR = 0;
    if (!idle) md770_rgb_latch();
    scan_rows();
    if (!idle) md770_rgb_update();
}

static const GPTConfig scan_config = {.frequency = 48000000, .callback = scan_tick};

void md770_scan_init(void) {
    for (uint8_t i = 0; i < 8; ++i) {
        gpio_write_pin_low(select_pins[i]);
        palSetLineMode(select_pins[i], PAL_MODE_HT32_AF(select_function(i)) | PAL_MODE_OUTPUT_PUSHPULL);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        palSetLineMode(PAL_LINE(GPIOA, i + SENSE_SHIFT), PAL_MODE_HT32_AF(0) | PAL_MODE_INPUT_PULLDOWN);
    }
    md770_rgb_init();
    gptStart(&GPTD_BFTM0, &scan_config);
    scan_tick(NULL);
    gptStartContinuous(&GPTD_BFTM0, scan_interval);
}

void md770_scan_copy(matrix_row_t rows[8]) {
    chSysLock();
    for (uint8_t i = 0; i < 8; ++i) {
        rows[i]      = scanned[i];
        last_rows[i] = stable_rows[i];
    }
    chSysUnlock();
}

void md770_scan_set_idle(bool asleep) {
    chSysLock();
    idle = asleep;
    chSysUnlock();
}

void md770_scan_pause(void) {
    if (GPTD_BFTM0.state == GPT_CONTINUOUS) gptStopTimer(&GPTD_BFTM0);
    if (GPTD_BFTM0.state == GPT_READY) gptStop(&GPTD_BFTM0);
}

void md770_scan_resume(void) {
    if (GPTD_BFTM0.state != GPT_STOP) return;
    gptStart(&GPTD_BFTM0, &scan_config);
    gptStartContinuous(&GPTD_BFTM0, scan_interval);
}

void md770_scan_idle_begin(void) {
    chSysLock();
    idle = true;
    memcpy(idle_rows, last_rows, sizeof(idle_rows));
    chSysUnlock();
}

bool md770_scan_idle_changed(bool *held) {
    bool changed = false;
    *held        = false;
    chSysLock();
    scan_rows();
    for (uint8_t row = 0; row <
#ifdef MD770_LEFT
                          8
#else
                          7
#endif
         ;
         ++row) {
        changed |= idle_rows[row] != stable_rows[row];
        *held |= debounce_count[row] || stable_rows[row] || idle_rows[row];
    }
    chSysUnlock();
    return changed;
}

void md770_scan_idle_normalise(void) {
    chSysLock();
    memset(idle_rows, 0, sizeof(idle_rows));
    chSysUnlock();
}

void md770_scan_prepare_wake(void) {
    md770_scan_pause();
    palSetGroupMode(GPIOA, SENSE_MASK, 0, PAL_MODE_HT32_AF(0) | PAL_MODE_INPUT_PULLDOWN);
#ifdef MD770_LEFT
    AFIO->ESSR[0] = 0;
    EXTI->WAKUPCR |= 0xff;
    uint8_t count = 8;
#else
    AFIO->ESSR[0] &= 0xfU;
    AFIO->ESSR[1] &= ~0xfU;
    EXTI->WAKUPCR |= 0x1fe;
    uint8_t count = 7;
    palSetLineMode(C1, PAL_MODE_HT32_AF(0) | PAL_HT32_MODE_PD);
#endif
    for (uint8_t row = 0; row < count; ++row) {
        gpio_write_pin_high(select_pins[row]);
        palSetLineMode(select_pins[row], PAL_MODE_HT32_AF(select_function(row)) | PAL_MODE_OUTPUT_PUSHPULL);
    }
}

void md770_scan_finish_wake(void) {
    for (uint8_t row = 0; row < 8; ++row) {
        gpio_write_pin_low(select_pins[row]);
        palSetLineMode(select_pins[row], PAL_MODE_HT32_AF(select_function(row)) | PAL_MODE_OUTPUT_PUSHPULL);
    }
    palClearPort(GPIOA, SENSE_MASK);
    GPIOA->DIRCR |= SENSE_MASK;
    if (!idle) md770_scan_resume();
}
