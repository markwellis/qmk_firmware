// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770.h"
#include "raw_hid.h"
#include "rgb_data.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif
#include <string.h>

#ifdef MD770_LEFT
#    define LED_CLOCK A14
#    define LED_LATCH A15
#    define CHANNELS 16
#    define ROW_SHIFT 0
#else
#    define LED_CLOCK C15
#    define LED_LATCH B10
#    define CHANNELS 32
#    define ROW_SHIFT 1
#endif
#define LED_DATA C12

static uint8_t          colours[MD770_RGB_SLOTS][3];
static uint8_t          wire_colours[MD770_RGB_SLOTS][3];
static uint16_t         channels[2][8][CHANNELS];
static volatile uint8_t active_buffer;
static uint8_t          row;
static uint8_t          startup_count = 255;
static bool             initialised;
static struct {
    uint32_t started;
    uint32_t duration;
    uint16_t on_ms;
    uint16_t off_ms;
    uint8_t  colour[3];
    bool     active;
    bool     on;
} alert;

static void clock_bit(bool high) {
    palClearLine(LED_CLOCK);
    palWriteLine(LED_DATA, high);
    palSetLine(LED_CLOCK);
}

static void shift_bits(uint16_t value, uint8_t count) {
    for (uint8_t bit = 0; bit < count; ++bit) {
        clock_bit(value & 0x8000);
        value <<= 1;
    }
}

static void control_clocks(uint8_t count) {
    palClearLine(LED_LATCH);
    palSetLine(LED_LATCH);
    for (uint8_t i = 0; i < count; ++i) {
        palClearLine(LED_CLOCK);
        palSetLine(LED_CLOCK);
    }
    palClearLine(LED_LATCH);
}

static void shift_row(uint8_t index) {
    if (startup_count && (--startup_count == 12 || startup_count == 8 || startup_count == 4 || startup_count == 0)) {
        control_clocks(18);
#ifndef MD770_LEFT
        shift_bits(0x0300, 10);
#endif
        shift_bits(0, 2);
        palClearLine(LED_CLOCK);
        palSetLine(LED_LATCH);
        shift_bits(0x0c00, 8);
        palSetLine(LED_DATA);
        palClearLine(LED_LATCH);
        palSetLine(LED_LATCH);
    }
    const uint16_t *values = channels[active_buffer][index];
    for (uint8_t group = 0; group < 16; ++group) {
        palClearLine(LED_LATCH);
#ifndef MD770_LEFT
        shift_bits(values[31 - 2 * group], 10);
        uint16_t value = values[30 - 2 * group];
#else
        uint16_t value = values[15 - group];
#endif
        shift_bits(value, 9);
        palClearLine(LED_CLOCK);
        palSetLine(LED_LATCH);
        palWriteLine(LED_DATA, (value & 0x0040) != 0);
        palSetLine(LED_CLOCK);
    }
    palSetLine(LED_DATA);
}

void md770_rgb_latch(void) {
    control_clocks(2);
    palSetLine(LED_LATCH);
}

void md770_rgb_update(void) {
    GPIOA->SRR = 1U << (row + ROW_SHIFT);
    GPTM1->CTR = 1;
    row        = (row + 1) & 7;
    shift_row(row);
}

void md770_rgb_init(void) {
    if (initialised) {
        return;
    }
    initialised = true;
    palSetLineMode(LED_DATA, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(LED_CLOCK, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(LED_LATCH, PAL_MODE_OUTPUT_PUSHPULL);
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
    palClearLine(C11);
    wait_us(100);
    shift_row(0);
}

static void publish(void) {
    uint8_t next = active_buffer ^ 1;
    memset(channels[next], 0, sizeof(channels[next]));
    for (uint8_t slot = 0; slot < MD770_RGB_SLOTS; ++slot) {
        for (uint8_t component = 0; component < 3; ++component) {
            wire_colours[slot][component] = alert.active ? (alert.on ? alert.colour[component] : 0) : colours[slot][component];
        }
    }
    for (uint8_t r = 0; r < 8; ++r) {
#ifdef MD770_LEFT
        for (uint8_t key = 0; key < 5; ++key) {
            uint8_t slot = hardware_slots[r][key];
            if (slot != 0xff) {
                for (uint8_t component = 0; component < 3; ++component) {
                    channels[next][r][component * 5 + key] = intensity[wire_colours[slot][component]];
                }
            }
        }
#else
        for (uint8_t key = 0; key < 7; ++key) {
            uint8_t slot = hardware_slots[r][key];
            if (slot != 0xff) {
                for (uint8_t component = 0; component < 3; ++component) {
                    uint8_t offset = 2 * key + 14 * component;
                    if (offset >= 32) {
                        offset -= 31;
                    }
                    channels[next][r][offset] = intensity[wire_colours[slot][component]];
                }
            }
        }
#endif
    }
    chSysLock();
    active_buffer = next;
    chSysUnlock();
}

static void set_colour(int index, uint8_t red, uint8_t green, uint8_t blue) {
#ifdef MD770_LEFT
    if (palReadLine(B8)) {
        return;
    }
#endif
    if ((unsigned)index < RGB_MATRIX_LED_COUNT) {
        uint8_t slot     = led_slots[index];
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
#ifdef MD770_LEFT
    if (palReadLine(B8)) {
        return;
    }
#endif
    publish();
}

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init          = md770_rgb_init,
    .flush         = flush,
    .set_color     = set_colour,
    .set_color_all = set_all,
};

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

void md770_rgb_task(void) {
    if (!alert.active) {
        return;
    }
    uint32_t elapsed = timer_elapsed32(alert.started);
    bool     on      = elapsed % ((uint32_t)alert.on_ms + alert.off_ms) < alert.on_ms;
    if (elapsed >= alert.duration) {
        alert.active = false;
        publish();
    } else if (on != alert.on) {
        alert.on = on;
        publish();
    }
}

static bool alert_command(const uint8_t *data, uint8_t length) {
    if (length < 14 || memcmp(data, "MD77", 4) != 0) {
        return false;
    }
    if (data[4] == 0) {
        alert.active = false;
        publish();
    } else if (data[4] == 1) {
        uint16_t on_ms  = data[8] | (uint16_t)data[9] << 8;
        uint16_t off_ms = data[10] | (uint16_t)data[11] << 8;
        uint16_t cycles = data[12] | (uint16_t)data[13] << 8;
        uint32_t period = (uint32_t)on_ms + off_ms;
        if (!on_ms || !off_ms || !cycles || cycles > 60000 / period) {
            return false;
        }
        uint32_t duration = period * cycles;
        alert.started     = timer_read32();
        alert.duration    = duration;
        alert.on_ms       = on_ms;
        alert.off_ms      = off_ms;
        alert.colour[0]   = MIN(data[5], RGB_MATRIX_MAXIMUM_BRIGHTNESS);
        alert.colour[1]   = MIN(data[6], RGB_MATRIX_MAXIMUM_BRIGHTNESS);
        alert.colour[2]   = MIN(data[7], RGB_MATRIX_MAXIMUM_BRIGHTNESS);
        alert.active      = true;
        alert.on          = true;
        publish();
    } else {
        return false;
    }
    return true;
}

#ifdef VIA_ENABLE
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (length < 2 || data[0] != id_custom_set_value || data[1] != id_custom_channel || !alert_command(data + 2, length - 2)) {
        data[0] = id_unhandled;
    }
}
#else
void raw_hid_receive(uint8_t *data, uint8_t length) {
    alert_command(data, length);
}
#endif
