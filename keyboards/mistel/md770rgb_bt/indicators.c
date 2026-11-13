// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"

#define INDICATOR_WIDTH 6000

static uint8_t layer_colour;
static bool    suspended;
static bool    initialised;

static void update_layer_led(void) {
    if (!initialised) {
        return;
    }
    pwmEnableChannel(&PWMD_GPTM0, 0, !suspended && (layer_colour & 1) ? INDICATOR_WIDTH : 0);
    pwmEnableChannel(&PWMD_GPTM0, 1, !suspended && (layer_colour & 2) ? INDICATOR_WIDTH : 0);
    pwmEnableChannel(&PWMD_MCTM0, 0, !suspended && (layer_colour & 4) ? INDICATOR_WIDTH : 0);
}

void led_init_ports(void) {
    static const PWMConfig gptm_config = {
        .frequency = 24000000,
        .period    = UINT16_MAX,
        .channels  = {{PWM_OUTPUT_ACTIVE_LOW, NULL}, {PWM_OUTPUT_ACTIVE_LOW, NULL}, {PWM_OUTPUT_ACTIVE_LOW, NULL}, {PWM_OUTPUT_DISABLED, NULL}},
    };
    static const PWMConfig mctm_config = {
        .frequency = 24000000,
        .period    = UINT16_MAX,
        .channels  = {{PWM_OUTPUT_ACTIVE_LOW, NULL}, {PWM_OUTPUT_DISABLED, NULL}, {PWM_OUTPUT_DISABLED, NULL}, {PWM_OUTPUT_DISABLED, NULL}},
    };
    pwmStart(&PWMD_GPTM0, &gptm_config);
    pwmStart(&PWMD_MCTM0, &mctm_config);
    initialised = true;
    pwmEnableChannel(&PWMD_GPTM0, 2, 0);
    update_layer_led();
    palSetLineMode(C4, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(C5, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(LED_CAPS_LOCK_PIN, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(A14, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
}

void led_update_ports(led_t led_state) {
    if (initialised) {
        pwmEnableChannel(&PWMD_GPTM0, 2, led_state.caps_lock ? INDICATOR_WIDTH : 0);
    }
}

void md770_set_layer_led(uint8_t colour) {
    layer_colour = colour;
    update_layer_led();
}

void suspend_power_down_kb(void) {
    suspended = true;
    update_layer_led();
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    suspended = false;
    update_layer_led();
    suspend_wakeup_init_user();
}

bool shutdown_kb(bool jump_to_bootloader) {
    if (shutdown_user(jump_to_bootloader) && initialised) {
        for (uint8_t i = 0; i < (jump_to_bootloader ? 3 : 1); ++i) {
            md770_set_layer_led(7);
            if (jump_to_bootloader) {
                led_update_ports((led_t){.caps_lock = true});
            }
            wait_ms((jump_to_bootloader ? 100 : 250));
            if (jump_to_bootloader) {
                md770_set_layer_led(0);
                led_update_ports((led_t){0});
                wait_ms(100);
            }
        }
    }
    return true;
}
