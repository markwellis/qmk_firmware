// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include <string.h>

#ifdef MD770_LEFT
#    define LED_CLOCK A14
#    define LED_LATCH A15
#    define LED_CHANNELS 16
#    define LED_ROW_SHIFT 0
#else
#    define LED_CLOCK C15
#    define LED_LATCH B10
#    define LED_CHANNELS 32
#    define LED_ROW_SHIFT 1
#endif
#define LED_DATA C12
#define UNUSED_SLOT UINT8_MAX

// Slot order is shared with the inter-half RGB protocol.
#ifdef RGB_MATRIX_ENABLE
static const uint8_t led_to_slot[] = {0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90, 1, 7, 13, 19, 25, 31, 37, 43, 49, 55, 61, 67, 73, 79, 91, 2, 8, 14, 20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 86, 92, 3, 9, 15, 21, 27, 33, 39, 45, 51, 57, 63, 69, 81, 93, 4, 16, 22, 28, 34, 40, 46, 52, 58, 64, 70, 82, 88, 94, 5, 11, 23, 35, 47, 65, 71, 77, 83, 89, 95};
STATIC_ASSERT(ARRAY_SIZE(led_to_slot) == RGB_MATRIX_LED_COUNT, "RGB slot map must cover every LED");
#endif

#ifdef MD770_LEFT
static const uint8_t scan_row_slots[8][5] = {
    {0, 6, 12, 18, 24}, {30, 36, UNUSED_SLOT, UNUSED_SLOT, UNUSED_SLOT}, {1, 2, 3, 4, 5}, {7, 8, 9, 16, 11}, {13, 14, 15, 22, 17}, {19, 20, 21, 28, 35}, {25, 26, 27, 34, 23}, {31, 32, 33, 40, 37},
};
#else
static const uint8_t scan_row_slots[8][7] = {
    {92, 38, 39, 46, 47, 42, 35}, {43, 44, 45, 52, 65, 48, 47}, {49, 50, 51, 58, 71, 54, 90}, {55, 56, 57, 64, 83, 60, 75}, {61, 62, 63, 70, 77, 66, UNUSED_SLOT}, {67, 68, 69, 82, 88, 72, UNUSED_SLOT}, {73, 74, 81, 95, 91, 78, UNUSED_SLOT}, {79, 86, 89, 93, 94, 84, UNUSED_SLOT},
};
#endif

static uint8_t          colours[MD770_RGB_SLOTS][3];
static uint8_t          wire_colours[MD770_RGB_SLOTS][3];
static uint16_t         pwm_buffers[2][8][LED_CHANNELS];
static volatile uint8_t active_buffer;
static uint8_t          row;
static uint8_t          startup_count = 255;
static bool             initialised;

static void publish(void);

static void clock_bit(bool high) {
    gpio_write_pin_low(LED_CLOCK);
    gpio_write_pin(LED_DATA, high);
    gpio_write_pin_high(LED_CLOCK);
}

static void shift_bits(uint16_t value, uint8_t count) {
    for (uint8_t bit = 0; bit < count; ++bit) {
        clock_bit(value & 0x8000);
        value <<= 1;
    }
}

static void control_clocks(uint8_t count) {
    gpio_write_pin_low(LED_LATCH);
    gpio_write_pin_high(LED_LATCH);
    for (uint8_t i = 0; i < count; ++i) {
        gpio_write_pin_low(LED_CLOCK);
        gpio_write_pin_high(LED_CLOCK);
    }
    gpio_write_pin_low(LED_LATCH);
}

static void shift_row(uint8_t index) {
    if (startup_count && (--startup_count == 12 || startup_count == 8 || startup_count == 4 || startup_count == 0)) {
        control_clocks(18);
#ifndef MD770_LEFT
        shift_bits(0x0300, 10);
#endif
        shift_bits(0, 2);
        gpio_write_pin_low(LED_CLOCK);
        gpio_write_pin_high(LED_LATCH);
        shift_bits(0x0c00, 8);
        gpio_write_pin_high(LED_DATA);
        gpio_write_pin_low(LED_LATCH);
        gpio_write_pin_high(LED_LATCH);
    }
    const uint16_t *values = pwm_buffers[active_buffer][index];
    for (uint8_t group = 0; group < 16; ++group) {
        gpio_write_pin_low(LED_LATCH);
#ifndef MD770_LEFT
        shift_bits(values[31 - 2 * group], 10);
        uint16_t value = values[30 - 2 * group];
#else
        uint16_t value = values[15 - group];
#endif
        shift_bits(value, 9);
        gpio_write_pin_low(LED_CLOCK);
        gpio_write_pin_high(LED_LATCH);
        gpio_write_pin(LED_DATA, (value & 0x0040) != 0);
        gpio_write_pin_high(LED_CLOCK);
    }
    gpio_write_pin_high(LED_DATA);
}

void md770_rgb_latch(void) {
    control_clocks(2);
    gpio_write_pin_high(LED_LATCH);
}

void md770_rgb_update(void) {
    gpio_write_pin_high(PAL_LINE(GPIOA, row + LED_ROW_SHIFT));
    GPTM1->CTR = 1;
    row        = (row + 1) & 7;
    shift_row(row);
}

void md770_rgb_init(void) {
    if (initialised) {
        return;
    }
    initialised = true;
    gpio_set_pin_output(LED_DATA);
    gpio_set_pin_output(LED_CLOCK);
    gpio_set_pin_output(LED_LATCH);
    palSetLineMode(C10, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    CKCU->APBCCR1 |= CKCU_APBCCR1_GPTM1EN;
    GPTM1->CTR        = 0;
    GPTM1->CNTCFR     = 0;
    GPTM1->MDCFR      = 0;
    GPTM1->TRCFR      = 0;
    GPTM1->CHPOLR     = 0;
    GPTM1->CHnOCFR[0] = 7;
    GPTM1->CHCTR      = 1;
    GPTM1->DICTR      = 0;
    GPTM1->CNTR       = 0;
    GPTM1->PSCR       = 0;
    GPTM1->CRR        = 2;
    GPTM1->CHnCCR[0]  = 1;
    wait_ms(10);
    gpio_write_pin_low(C11);
    wait_us(100);
    publish();
    shift_row(0);
}

static void publish(void) {
    uint8_t next = active_buffer ^ 1;
    memset(pwm_buffers[next], 0, sizeof(pwm_buffers[next]));
    memcpy(wire_colours, colours, sizeof(wire_colours));
    // RGB Matrix already applies its lightness curve.
    for (uint8_t r = 0; r < 8; ++r) {
#ifdef MD770_LEFT
        for (uint8_t key = 0; key < 5; ++key) {
            uint8_t slot = scan_row_slots[r][key];
            if (slot != UNUSED_SLOT) {
                for (uint8_t component = 0; component < 3; ++component) {
                    pwm_buffers[next][r][component * 5 + key] = (uint16_t)wire_colours[slot][component] * (UINT16_MAX / UINT8_MAX);
                }
            }
        }
#else
        for (uint8_t key = 0; key < 7; ++key) {
            uint8_t slot = scan_row_slots[r][key];
            if (slot != UNUSED_SLOT) {
                for (uint8_t component = 0; component < 3; ++component) {
                    uint8_t offset = 2 * key + 14 * component;
                    if (offset >= 32) {
                        offset -= 31;
                    }
                    pwm_buffers[next][r][offset] = (uint16_t)wire_colours[slot][component] * (UINT16_MAX / UINT8_MAX);
                }
            }
        }
#endif
    }
    chSysLock();
    active_buffer = next;
    chSysUnlock();
}

#ifdef RGB_MATRIX_ENABLE
static void set_colour(int index, uint8_t red, uint8_t green, uint8_t blue) {
#    ifdef MD770_LEFT
    if (gpio_read_pin(B8)) {
        return;
    }
#    endif
    if ((unsigned)index < RGB_MATRIX_LED_COUNT) {
        uint8_t slot     = led_to_slot[index];
        colours[slot][0] = red;
        colours[slot][1] = green;
        colours[slot][2] = blue;
    }
}

static void set_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; ++i) {
        set_colour(i, red, green, blue);
    }
}

static void flush(void) {
#    ifdef MD770_LEFT
    if (gpio_read_pin(B8)) {
        return;
    }
#    endif
    publish();
}

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init          = md770_rgb_init,
    .flush         = flush,
    .set_color     = set_colour,
    .set_color_all = set_all,
};
#endif

const uint8_t *md770_rgb_left(void) {
    return &wire_colours[0][0];
}

void md770_rgb_receive(const uint8_t *payload) {
#ifdef MD770_LEFT
    memcpy(colours, payload, MD770_LEFT_RGB_SLOTS * 3);
    publish();
#else
    (void)payload;
#endif
}
