// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "test_common.h"
#include <stddef.h>
#include <stdint.h>
#undef MATRIX_ROWS
#define MATRIX_ROWS 16
#define B0 0
#define B1 1
#define B8 8
#define PAL_MODE_HT32_AF(n) 0
#define PAL_MODE_OUTPUT_PUSHPULL 0
#define PAL_MODE_INPUT 0
#define TIME_IMMEDIATE 0
#define MSG_OK 0
#define palSetLineMode(pin, mode) ((void)0)
#define gpio_write_pin_high(pin) ((void)0)
#define gpio_set_pin_input_low(pin) ((void)0)
#define gpio_read_pin(pin) true
#define chSysLock() ((void)0)
#define chSysUnlock() ((void)0)
typedef int msg_t;
typedef struct {
    unsigned speed;
} SerialConfig;
typedef struct {
    unsigned oqueue;
} SerialDriver;
#ifdef __cplusplus
extern "C" {
#endif
extern SerialDriver SD0;
void                sdStart(SerialDriver *driver, const SerialConfig *config);
msg_t               sdGetTimeout(SerialDriver *driver, unsigned timeout);
size_t              oqGetEmptyI(unsigned *queue);
size_t              sdWriteTimeout(SerialDriver *driver, const uint8_t *data, size_t length, unsigned timeout);

#ifdef __cplusplus
}
#endif

#define USART_SIFR_TXC (1U << 8)
#define oqIsEmptyI(queue) true
#define chVTGetSystemTimeX() (timer_read32() * 2U)
typedef uint32_t systime_t;
static struct {
    uint32_t CR, SIFR;
} md770_test_usart __attribute__((unused));
static struct {
    uint32_t APBPRSTR1;
} md770_test_reset __attribute__((unused));
#define USART0 (&md770_test_usart)
#define RSTCU (&md770_test_reset)
#ifdef __cplusplus
extern "C" {
#endif
extern int USBD1;
void       sdStop(SerialDriver *driver);
void       usbConnectBus(void *driver);
void       usbDisconnectBus(void *driver);
#ifdef __cplusplus
}
#endif
