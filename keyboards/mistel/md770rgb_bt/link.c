// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "protocol.h"
#ifdef MD770_LEFT
#    include "connection.h"
#endif
#include <string.h>

static md770_parser_t parser;
static systime_t      last_ticks;
static uint8_t        send_ticks;
static bool           sleeping;
#ifdef MD770_LEFT
static uint8_t mode, mode_count;
static bool    rgb_active;
static uint8_t last_keys[8];
#endif
static const SerialConfig serial_config = {.speed = 500000};

static void set_tx_active(void) {
    gpio_write_pin_high(B0);
    palSetLineMode(B0, PAL_MODE_HT32_AF(6) | PAL_MODE_OUTPUT_PUSHPULL);
}

void md770_link_init(void) {
    set_tx_active();
    palSetLineMode(B1, PAL_MODE_HT32_AF(6) | PAL_MODE_INPUT);
#ifdef MD770_LEFT
    gpio_set_pin_input_low(B8);
#else
    gpio_set_pin_input_high(B8);
    gpio_write_pin_high(B11);
    palSetLineMode(B11, PAL_MODE_HT32_AF(1) | PAL_MODE_OUTPUT_PUSHPULL | PAL_MODE_INPUT);
#endif
    sdStart(&SD0, &serial_config);
    last_ticks = chVTGetSystemTimeX();
    send_ticks = 0;
#ifdef MD770_LEFT
    memset(last_keys, 0xff, sizeof(last_keys));
#endif
}

bool md770_link_can_sleep(void) {
    return sleeping || (oqIsEmptyI(&SD0.oqueue) && (USART0->SIFR & USART_SIFR_TXC));
}

void md770_link_sleep(bool asleep) {
    if (sleeping == asleep) return;
    if (asleep) {
        sdStop(&SD0);
        memset(&parser, 0, sizeof(parser));
        chSysLock();

        USART0->CR       = 0;
        RSTCU->APBPRSTR1 = 1U << 8;
        while (RSTCU->APBPRSTR1 & (1U << 8)) {
        }
        palSetLineMode(B0, PAL_MODE_HT32_AF(0));
        sleeping = true;
        chSysUnlock();
    } else {
        chSysLock();
        set_tx_active();
        sleeping = false;
        chSysUnlock();
        sdStart(&SD0, &serial_config);
        last_ticks = chVTGetSystemTimeX();
        send_ticks = 0;
    }
}

bool md770_link_connected(void) {
#ifdef MD770_LEFT
    return mode == 2;
#else
    return true;
#endif
}

bool md770_link_rgb_active(void) {
#ifdef MD770_LEFT
    return rgb_active;
#else
    return true;
#endif
}

#ifdef MD770_LEFT
static void receive_rgb(const uint8_t *payload) {
    bool nonzero = false;
    for (uint8_t i = 0; i < 126; ++i)
        nonzero |= payload[i] != 0;
    md770_rgb_receive(payload);
    if (nonzero) md770_power_activity(false);
    rgb_active = nonzero;
}

static void mode_task(void) {
    if (gpio_read_pin(B8)) {
        if (mode < 2) {
            ++mode_count;
            if (mode_count == 16) {
                mode = 2;
                connection_set_host_noeeprom(CONNECTION_HOST_NONE);
                usbDisconnectBus(&USBD1);
            }
        } else if (mode_count < 16)
            ++mode_count;
    } else {
        if (mode != 1) {
            --mode_count;
            if (!mode_count) {
                mode = 1;
                if (rgb_active) md770_rgb_sleep(true);
                rgb_active = false;
                usbConnectBus(&USBD1);
                connection_set_host_noeeprom(CONNECTION_HOST_USB);
            }
        } else if (mode_count)
            --mode_count;
    }
}
#endif

static void send_snapshot(void) {
#ifdef MD770_LEFT
    uint8_t keys[8];
    for (uint8_t i = 0; i < 8; ++i)
        keys[i] = (uint8_t)~matrix_get_row(8 + i);
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
    if (!space || sdWriteTimeout(&SD0, frame, frame_length, TIME_IMMEDIATE) != frame_length) return;
#ifdef MD770_LEFT
    memcpy(last_keys, keys, sizeof(last_keys));
#endif
}

void md770_link_task(matrix_row_t rows[MATRIX_ROWS]) {
    if (sleeping) return;
#ifdef MD770_LEFT
    uint8_t payload[126];
#else
    uint8_t payload[8];
#endif
    msg_t byte;
    while ((byte = sdGetTimeout(&SD0, TIME_IMMEDIATE)) >= MSG_OK) {
        if (md770_receive(&parser, byte, payload, sizeof(payload))) {
#ifdef MD770_LEFT
            receive_rgb(payload);
#else
            for (uint8_t i = 0; i < 8; ++i)
                rows[8 + i] = (uint8_t)~payload[i];
#endif
        }
    }
    systime_t ticks   = chVTGetSystemTimeX();
    systime_t elapsed = ticks - last_ticks;
    last_ticks        = ticks;
#ifdef MD770_LEFT
    if (mode == 2) {
        bool periodic = elapsed && !(++send_ticks & 63);
        bool changed  = false;
        for (uint8_t i = 0; i < 8; ++i) {
            changed |= last_keys[i] != (uint8_t)~matrix_get_row(8 + i);
        }
        if (changed || periodic) send_snapshot();
    }
    mode_task();
#else
    if (elapsed) {
        send_ticks += elapsed;
        if (send_ticks >= 64) {
            send_ticks = 0;
            send_snapshot();
        }
    }
#endif
}
