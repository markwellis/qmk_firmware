// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "bluetooth.h"
#include "connection.h"

#define INDICATOR_WIDTH 6000

static uint8_t           layer_colour;
static bool              suspended;
static bool              initialised;
static bool              caps;
static volatile uint32_t caps_started;
static uint32_t          last_ticks;
static uint16_t          radio_phases, radio_interval, radio_countdown, burst_countdown;
static uint8_t           burst_phase_count;
static bool              radio_mode, radio_phase, pairing, burst_phase;
static bool              red_output, blue_output;

uint32_t __attribute__((noinline, noclone)) md770_indicator_ticks(void) {
    return chVTGetSystemTimeX();
}

static uint16_t elapsed_countdown(uint16_t remaining, uint32_t elapsed) {
    return elapsed >= remaining ? 0 : remaining - elapsed;
}

static void update_layer_led(void) {
    if (!initialised) {
        return;
    }
    pwmEnableChannel(&PWMD_GPTM0, 0, !suspended && (layer_colour & 1) ? INDICATOR_WIDTH : 0);
    pwmEnableChannel(&PWMD_GPTM0, 1, !suspended && (layer_colour & 2) ? INDICATOR_WIDTH : 0);
    pwmEnableChannel(&PWMD_MCTM0, 0, !suspended && (layer_colour & 4) ? INDICATOR_WIDTH : 0);
}

void md770_indicators_sleep(bool asleep) {
    suspended = asleep;
    update_layer_led();
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
        .channels  = {{PWM_OUTPUT_ACTIVE_LOW, NULL}, {PWM_OUTPUT_DISABLED, NULL}, {PWM_OUTPUT_DISABLED, NULL}, {PWM_OUTPUT_ACTIVE_LOW, NULL}},
    };
    pwmStart(&PWMD_GPTM0, &gptm_config);
    pwmStart(&PWMD_MCTM0, &mctm_config);
    initialised = true;
    pwmEnableChannel(&PWMD_GPTM0, 2, 0);
    pwmEnableChannel(&PWMD_MCTM0, 3, 0);
    last_ticks = md770_indicator_ticks();
    update_layer_led();
    palSetLineMode(C4, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(C5, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(LED_CAPS_LOCK_PIN, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(A14, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
    palSetLineMode(D3, PAL_MODE_HT32_AF(4) | PAL_MODE_OUTPUT_PUSHPULL);
}

void led_update_ports(led_t led_state) {
    caps = led_state.caps_lock;
    md770_indicators_task();
}

void md770_radio_led_update(void) {
    caps_started = md770_indicator_ticks();
}

void md770_radio_release_keep_awake(void) {
    pairing = false;
}

void md770_radio_cancel_indication(void) {
    radio_phases = 0;
    pairing      = false;
}

void md770_radio_indicate(bool pair) {
    radio_mode     = true;
    pairing        = pair;
    radio_phases   = pair ? 680 : 4;
    radio_interval = pair ? 1000 : 500;
}

void md770_indicators_task(void) {
    if (!initialised) return;
    uint32_t ticks   = md770_indicator_ticks();
    uint32_t elapsed = ticks - last_ticks;
    last_ticks       = ticks;
    radio_countdown  = elapsed_countdown(radio_countdown, elapsed);
    burst_countdown  = elapsed_countdown(burst_countdown, elapsed);
    suspended        = md770_power_idle() || md770_power_usb_suspended();
    update_layer_led();
    if (radio_phases && bluetooth_is_connected()) md770_radio_cancel_indication();
    if (radio_mode) {
        if (!radio_phases) {
            red_output = blue_output = radio_mode = false;
            if (pairing) md770_power_limit_awake(3000);
            pairing = false;
        } else {
            if (pairing) md770_power_keep_awake(120000);
            if (!radio_countdown) {
                radio_countdown = radio_interval;
                --radio_phases;
                radio_phase = !radio_phase;
                red_output  = false;
                blue_output = radio_phase;
            }
        }
    } else if (md770_charge_indicated() && gpio_read_pin(C13)) {
        blue_output = true;
    } else if (md770_charge_indicated() || md770_battery_low()) {
        if (!burst_countdown) {
            burst_phase       = !burst_phase;
            burst_phase_count = (burst_phase_count + 1) % 6;
            burst_countdown   = burst_phase_count ? 50 : 6000;
            if (md770_charge_indicated())
                blue_output = burst_phase;
            else
                red_output = burst_phase;
        }
    } else {
        blue_output = false;
    }
    if (!md770_battery_low()) {
        red_output = caps;
        if (connection_get_host() == CONNECTION_HOST_BLUETOOTH) {
            uint32_t caps_elapsed = ticks - caps_started;
            red_output            = (bluetooth_keyboard_leds() & 2) && caps_elapsed < 4000 && ((4000 - caps_elapsed) & 1024);
        }
        if (suspended) red_output = false;
    }
    if (md770_power_idle()) red_output = blue_output = false;
    pwmEnableChannel(&PWMD_GPTM0, 2, red_output ? INDICATOR_WIDTH : 0);
    pwmEnableChannel(&PWMD_MCTM0, 3, blue_output ? INDICATOR_WIDTH : 0);
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
        md770_radio_shutdown();
        for (uint8_t i = 0; i < (jump_to_bootloader ? 3 : 5); ++i) {
            md770_set_layer_led(7);
            if (jump_to_bootloader) {
                pwmEnableChannel(&PWMD_GPTM0, 2, INDICATOR_WIDTH);
            }
            wait_ms(100);
            md770_set_layer_led(0);
            if (jump_to_bootloader) {
                pwmEnableChannel(&PWMD_GPTM0, 2, 0);
            }
            wait_ms(100);
        }
    }
    return true;
}
