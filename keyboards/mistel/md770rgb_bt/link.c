// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "protocol.h"
#include <string.h>

static md770_parser_t     parser;
static uint32_t           last_sent;
static uint32_t           last_received;
static const SerialConfig serial_config = {.speed = 500000};

void md770_link_init(void) {
    palSetLineMode(B0, PAL_MODE_HT32_AF(6) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(B1, PAL_MODE_HT32_AF(6) | PAL_MODE_INPUT);
#ifdef MD770_LEFT
    gpio_set_pin_input_low(B8);
#else
    gpio_set_pin_input_high(B8);
    gpio_write_pin_high(B11);
    palSetLineMode(B11, PAL_MODE_OUTPUT_PUSHPULL | PAL_MODE_INPUT);
#endif
    sdStart(&SD0, &serial_config);
}

void md770_link_task(matrix_row_t rows[MATRIX_ROWS]) {
#ifdef MD770_LEFT
    uint8_t payload[126];
#else
    uint8_t payload[8];
#endif
    msg_t byte;
    while ((byte = sdGetTimeout(&SD0, TIME_IMMEDIATE)) >= MSG_OK) {
        if (md770_receive(&parser, byte, payload, sizeof(payload))) {
            last_received = timer_read32();
#ifdef MD770_LEFT
            md770_rgb_receive(payload);
#else
            for (uint8_t i = 0; i < 8; ++i) {
                rows[8 + i] = (uint8_t)~payload[i];
            }
#endif
        }
    }
#ifndef MD770_LEFT
    if (timer_elapsed32(last_received) >= 100) {
        memset(rows + 8, 0, 8 * sizeof(matrix_row_t));
    }
#endif
    if (timer_elapsed32(last_sent) < 32) {
        return;
    }
#ifdef MD770_LEFT
    if (!gpio_read_pin(B8)) {
        return;
    }
    uint8_t keys[8];
    for (uint8_t i = 0; i < 8; ++i) {
        keys[i] = (uint8_t)~matrix_get_row(8 + i);
    }
    const uint8_t *out    = keys;
    uint8_t        length = sizeof(keys);
#else
    const uint8_t *out    = md770_rgb_left();
    uint8_t        length = 126;
#endif
    uint8_t frame[MD770_FRAME_MAX];
    size_t  frame_length = md770_encode(frame, out, length);
    chSysLock();
    bool space = oqGetEmptyI(&SD0.oqueue) >= frame_length;
    chSysUnlock();
    if (space && sdWriteTimeout(&SD0, frame, frame_length, TIME_IMMEDIATE) == frame_length) {
        last_sent = timer_read32();
    }
}
