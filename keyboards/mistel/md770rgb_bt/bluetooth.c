// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bluetooth.h"
#include "connection.h"
#include "md770rgb_bt.h"
#include <string.h>

enum radio_phase { RESET_HIGH, RESET_LOW, READY, TRANSMITTING, FAILED, OFF };
enum radio_command {
    COMMAND_STOP         = 0x00,
    NO_COMMAND           = 0x20,
    COMMAND_SELECT_SLOT1 = 0x41,
    COMMAND_SELECT_SLOT2 = 0x42,
    COMMAND_SELECT_SLOT3 = 0x43,
    COMMAND_PAIR_START   = 0x50,
    COMMAND_PAIR_SLOT1   = 0x51,
    COMMAND_PAIR_SLOT2   = 0x52,
    COMMAND_PAIR_SLOT3   = 0x53,
    COMMAND_USB_STATUS   = 0x80,
    COMMAND_KEYBOARD     = 0xf0,
    COMMAND_CONSUMER     = 0xf1,
    COMMAND_SYSTEM       = 0xf2,
};
enum { RX_SIZE = 10, KEYBOARD = 0xa1, CONSUMER = 0xa2, SYSTEM = 0xa3, SLOT_CONTROL = 0xaa, USB_STATUS = 0xa7 };

enum { REPORT_QUEUE_SIZE = 16 };
static struct radio_report {
    uint8_t length, data[9];
} reports[REPORT_QUEUE_SIZE];
static uint8_t                   report_head, report_count;
static enum radio_command        active_command;
static volatile uint8_t          tx[9], rx[RX_SIZE], tx_length, tx_position, rx_length;
static volatile bool             selected, failed, parsed;
static volatile uint8_t          radio_state, radio_leds, startup_attempts;
static volatile enum radio_phase phase;
static uint32_t                  phase_started, ready_at, held_started;
static uint32_t                  sleep_guard;
static volatile bool             radio_suspended;
static enum radio_command        pending_command;
static uint8_t                   held_slot, deferred_slot, saved_slot;
static keypos_t                  held_key;
static bool                      held_armed;
static volatile bool             stopped;

static bool receive_status(const uint8_t *data, uint8_t length);
static void radio_task(void);

void md770_radio_tick(void) {
    if (stopped || radio_suspended || phase == OFF || phase == FAILED) return;
    if (phase == RESET_HIGH || phase == RESET_LOW || phase == TRANSMITTING || (phase == READY && parsed)) md770_power_radio_activity();
}

static bool all_keys_up(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        if (row != 7 && matrix_get_row(row)) return false;
    }
    return true;
}

static void service_fifo(void) {
    for (uint8_t i = 0; i < 8 && (SPI0->FSR & 0xf0); ++i) {
        uint8_t byte = SPI0->DR;
        if (rx_length < RX_SIZE) rx[rx_length++] = byte;
        if (!parsed) {
            parsed = receive_status((const uint8_t *)rx, rx_length);
        }
    }
    for (uint8_t i = 0; i < 8 && tx_position < tx_length && (SPI0->FSR & 0x0f) < 7; ++i) {
        SPI0->DR = tx[tx_position++];
    }
    selected |= rx_length != 0 || (SPI0->SR & 0x100) != 0;
}

OSAL_IRQ_HANDLER(HT32_SPI0_IRQ_VECTOR) {
    OSAL_IRQ_PROLOGUE();
    if (!radio_suspended && (CKCU->APBCCR0 & CKCU_APBCCR0_SPI0EN) && (SPI0->CR0 & 1)) {
        if (SPI0->SR & 0xf8) {
            failed    = true;
            SPI0->IER = 0;
            SPI0->SR  = 0xff;
        } else {
            service_fifo();
        }
    }
    OSAL_IRQ_EPILOGUE();
}

static void arm_receiver(void) {
    CKCU->APBCCR0 |= CKCU_APBCCR0_SPI0EN;
    SPI0->IER        = 0;
    RSTCU->APBPRSTR0 = RSTCU_APBPRSTR0_SPI0RST;
    while (RSTCU->APBPRSTR0 & RSTCU_APBPRSTR0_SPI0RST) {
    }
    SPI0->CR1 = 0x0108;
    SPI0->CPR = 0x17;
    SPI0->FCR = 0x410;
    SPI0->CR0 = 1;
    tx_length = tx_position = rx_length = 0;
    selected = failed = parsed = false;
    SPI0->IER                  = 0xfc;
}

void md770_radio_power_downI(void) {
    report_head = report_count = 0;
    pending_command            = COMMAND_USB_STATUS;
    sleep_guard                = 10;
    arm_receiver();
    phase     = READY;
    SPI0->IER = 0;
    SPI0->CR0 = 0;
    CKCU->APBCCR0 &= ~CKCU_APBCCR0_SPI0EN;
    palSetGroupMode(GPIOB, 0x3c, 0, PAL_MODE_HT32_AF(0));
    radio_suspended = true;
}

void md770_radio_power_upI(void) {
    palSetGroupMode(GPIOB, 0x3c, 0, PAL_MODE_HT32_AF(5) | PAL_MODE_INPUT);
    arm_receiver();
    ready_at        = timer_read32() + sleep_guard;
    radio_suspended = false;
}

uint8_t md770_radio_state(void) {
    return radio_state;
}

static void delay_frame(uint16_t milliseconds) {
    uint32_t now = timer_read32();
    if ((int32_t)(ready_at - now) < milliseconds) ready_at = now + milliseconds;
}

static void rearm(void) {
    chSysLock();
    arm_receiver();
    chSysUnlock();
    phase = READY;
    if (pending_command == COMMAND_PAIR_START)
        delay_frame(10);
    else
        ready_at = timer_read32() + 10;
    md770_power_activity(false);
}

static void pulse_radio(void) {
    chSysLock();
    if (CKCU->APBCCR0 & CKCU_APBCCR0_SPI0EN) {
        SPI0->IER = 0;
        SPI0->CR0 = 0;
    }
    gpio_write_pin_low(C14);
    gpio_write_pin_high(D1);
    chSysUnlock();
    ready_at      = timer_read32();
    phase         = RESET_HIGH;
    phase_started = timer_read32();
}

static void reset_radio(void) {
    radio_state      = 0;
    startup_attempts = 4;
    if (pending_command == COMMAND_STOP) pending_command = COMMAND_USB_STATUS;
    pulse_radio();
}

bool md770_radio_key_ready(uint16_t code) {
    (void)code;
    return !stopped && !radio_suspended && (phase == READY || phase == TRANSMITTING) && radio_state == 4 && report_count < REPORT_QUEUE_SIZE;
}

__attribute__((weak)) void md770_playback_cancel_physical(void) {}

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && !IS_QK_MACRO(keycode)) md770_playback_cancel_physical();
    return pre_process_record_user(keycode, record);
}

void md770_radio_discard_reports(void) {
    if (phase == TRANSMITTING && active_command >= COMMAND_KEYBOARD && active_command == pending_command)
        report_count = 1;
    else {
        report_count = 0;
        if (pending_command >= COMMAND_KEYBOARD) pending_command = NO_COMMAND;
    }
}

void md770_radio_sync_reports(void) {
    if (connection_get_host() != CONNECTION_HOST_BLUETOOTH) return;
    bluetooth_send_keyboard(keyboard_report);
    bluetooth_send_consumer(host_last_consumer_usage());
    bluetooth_send_system(host_last_system_usage());
}

void md770_radio_release_reports(void) {
    md770_radio_discard_reports();
    report_keyboard_t empty = {0};
    bluetooth_send_keyboard(&empty);
    bluetooth_send_consumer(0);
    bluetooth_send_system(0);
}

static void append_report(const uint8_t *data, uint8_t length) {
    uint8_t tail         = (report_head + report_count) % REPORT_QUEUE_SIZE;
    reports[tail].length = length;
    memcpy(reports[tail].data, data, length);
    ++report_count;
}

static void queue_report(const uint8_t *data, uint8_t length) {
    if (stopped || radio_suspended || phase == FAILED || phase == OFF) return;
    if (report_count == REPORT_QUEUE_SIZE) {
        // Keep an in-flight packet stable and release every report family after
        // overflow.
        report_count              = pending_command >= COMMAND_KEYBOARD ? 1 : 0;
        const uint8_t keyboard[9] = {KEYBOARD}, consumer[5] = {CONSUMER}, system[2] = {SYSTEM};
        append_report(keyboard, sizeof(keyboard));
        append_report(consumer, sizeof(consumer));
        append_report(system, sizeof(system));
    }
    append_report(data, length);
    md770_power_activity(true);
}

void bluetooth_send_keyboard(report_keyboard_t *report) {
    uint8_t packet[9] = {KEYBOARD, report->mods, 0};
    memcpy(packet + 3, report->keys, 6);
    queue_report(packet, sizeof(packet));
}

static const uint16_t consumer_usages[] = {0x183, 0x18a, 0x192, 0x194, 0xcd, 0xb7, 0xb6, 0xb5, 0xe2, 0xea, 0xe9, 0x221, 0x223, 0x224, 0x225, 0x226, 0x227, 0x22a};

void bluetooth_send_consumer(uint16_t usage) {
    uint8_t packet[5] = {CONSUMER};
    for (uint8_t i = 0; i < ARRAY_SIZE(consumer_usages); ++i) {
        if (consumer_usages[i] == usage) packet[1 + i / 8] = 1U << (i % 8);
    }
    queue_report(packet, sizeof(packet));
}

void bluetooth_send_system(uint16_t usage) {
    uint8_t packet[2] = {SYSTEM, usage >= 0x81 && usage <= 0x83 ? (uint8_t)(1U << (usage - 0x81)) : (uint8_t)0};
    queue_report(packet, sizeof(packet));
}

bool bluetooth_is_connected(void) {
    return radio_state == 4;
}

uint8_t bluetooth_keyboard_leds(void) {
    return radio_leds;
}

bool md770_radio_usb_allowed(void) {
    return radio_state < 2 || !gpio_read_pin(A10);
}

static bool receive_status(const uint8_t *data, uint8_t length) {
    if (!length || !data[0]) return false;
    if (data[0] == 0xb1) {
        if (length < 2) return false;
        radio_leds = data[1];
        md770_radio_led_update();
    } else if (data[0] == 0xb6) {
        if (length < 3 || data[1] > 3) return false;
        switch (data[1]) {
            case 0:
                radio_state = data[2] + 2;
                break;
            case 1:
                radio_leds = data[2];
                md770_radio_led_update();
                break;
            case 2:
                startup_attempts = data[2];
                break;
            case 3:
                break;
        }
    } else if (data[0] == 0xb7) {
        if (length < 2 || data[1] != 2) return false;
    }
    return true;
}

static bool start_packet(const uint8_t *data, uint8_t length) {
    chSysLock();
    if (selected || rx_length || (SPI0->SR & 0x100)) {
        chSysUnlock();
        return false;
    }
    for (uint8_t i = 0; i < length; ++i)
        tx[i] = data[i];
    tx_length     = length;
    phase         = TRANSMITTING;
    phase_started = timer_read32();
    service_fifo();
    gpio_write_pin_high(C14);
    chSysUnlock();
    return true;
}

void md770_radio_select(uint8_t slot, bool pair) {
    if (slot < 1 || slot > 3 || stopped) return;
    bool restarting = phase == FAILED || phase == OFF;
    bool accepted   = radio_state >= 2;
    if (!accepted && pending_command >= COMMAND_KEYBOARD) pending_command = NO_COMMAND;
    if (accepted) {
        if (slot != saved_slot) {
            saved_slot = slot;
            eeconfig_update_kb(slot);
        }
        pending_command = (enum radio_command)((pair ? COMMAND_PAIR_SLOT1 : COMMAND_SELECT_SLOT1) + slot - 1);
        radio_state     = pair ? 3 : 2;
        if (!pair) startup_attempts = 0;
    }
    md770_playback_cancel_physical();
    report_head = report_count = 0;
    clear_keyboard();
    deferred_slot = 0;
    connection_set_host(CONNECTION_HOST_BLUETOOTH);
    md770_radio_indicate(pair);
    md770_power_activity(true);
    if (restarting && accepted) reset_radio();
}

__attribute__((weak)) bool md770_radio_pairing_chord(void) {
    return false;
}

void md770_radio_matrix_changed(bool pressed) {
    held_armed   = pressed;
    held_started = timer_read32();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) return false;
    if (!record->event.pressed && held_slot && record->event.key.row == held_key.row && record->event.key.col == held_key.col) held_slot = 0;
    if (keycode < QK_BLUETOOTH_PROFILE1 || keycode > QK_BLUETOOTH_PROFILE3) return true;
    if (record->event.pressed) {
        held_slot = keycode - QK_BLUETOOTH_PROFILE1 + 1;
        if (connection_get_host() == CONNECTION_HOST_BLUETOOTH) deferred_slot = held_slot;
        held_key     = record->event.key;
        held_started = timer_read32();
        held_armed   = true;
    }
    return false;
}

void bluetooth_init(void) {
    uint32_t slot = eeconfig_read_kb();
    saved_slot    = slot >= 1 && slot <= 3 ? slot : 1;
    deferred_slot = saved_slot;
    gpio_write_pin_low(C14);
    gpio_set_pin_output(C14);
    gpio_write_pin_high(D1);
    gpio_set_pin_output(D1);
    gpio_set_pin_input_low(A10);
    palSetGroupMode(GPIOB, 0x3c, 0, PAL_MODE_HT32_AF(5) | PAL_MODE_INPUT);
    CKCU->APBCCR0 |= CKCU_APBCCR0_SPI0EN;
    nvicEnableVector(SPI0_IRQn, MD770_IRQ_PRIORITY(1));
    pending_command = COMMAND_USB_STATUS;
    reset_radio();
}

static void build_packet(void) {
    if (pending_command == NO_COMMAND && report_count) {
        uint8_t family  = reports[report_head].data[0];
        pending_command = family == KEYBOARD ? COMMAND_KEYBOARD : family == CONSUMER ? COMMAND_CONSUMER : COMMAND_SYSTEM;
    }
    uint8_t packet[9], length = 0;
    switch (pending_command) {
        case COMMAND_STOP:
        case NO_COMMAND:
            break;
        case COMMAND_KEYBOARD:
        case COMMAND_CONSUMER:
        case COMMAND_SYSTEM:
            length = reports[report_head].length;
            memcpy(packet, reports[report_head].data, length);
            break;
        case COMMAND_SELECT_SLOT1:
        case COMMAND_SELECT_SLOT2:
        case COMMAND_SELECT_SLOT3:
        case COMMAND_PAIR_SLOT1:
        case COMMAND_PAIR_SLOT2:
        case COMMAND_PAIR_SLOT3:
            packet[0] = SLOT_CONTROL;
            packet[1] = 3;
            packet[2] = pending_command & 0xf;
            length    = 3;
            break;
        case COMMAND_PAIR_START:
            packet[0] = SLOT_CONTROL;
            packet[1] = 0;
            packet[2] = 1;
            length    = 3;
            break;
        case COMMAND_USB_STATUS:
            packet[0] = USB_STATUS;
            packet[1] = 2;
            packet[2] = 0;
            packet[3] = md770_power_usb_ready() ? 1 : 2;
            length    = 4;
            break;
    }
    if (length && start_packet(packet, length)) active_command = pending_command;
}

static void complete_command(uint8_t length) {
    if (active_command != pending_command) return;
    uint8_t minimum = 0;
    switch (pending_command) {
        case COMMAND_STOP:
        case NO_COMMAND:
            break;
        case COMMAND_KEYBOARD:
            minimum = 9;
            break;
        case COMMAND_CONSUMER:
            minimum = 5;
            break;
        case COMMAND_SYSTEM:
            minimum = 2;
            break;
        case COMMAND_USB_STATUS:
            minimum = 4;
            break;
        case COMMAND_SELECT_SLOT1:
        case COMMAND_SELECT_SLOT2:
        case COMMAND_SELECT_SLOT3:
        case COMMAND_PAIR_START:
        case COMMAND_PAIR_SLOT1:
        case COMMAND_PAIR_SLOT2:
        case COMMAND_PAIR_SLOT3:
            minimum = 3;
            break;
    }
    if (!minimum || length < minimum) return;
    if (pending_command >= COMMAND_KEYBOARD) {
        report_head = (report_head + 1) % REPORT_QUEUE_SIZE;
        --report_count;
    }
    if (pending_command >= COMMAND_PAIR_SLOT1 && pending_command <= COMMAND_PAIR_SLOT3) {
        pending_command = COMMAND_PAIR_START;
        delay_frame(150);
    } else {
        pending_command = NO_COMMAND;
    }
}

static void __attribute__((noinline, noclone)) radio_task(void) {
    if (deferred_slot && !held_slot && all_keys_up() && radio_state >= 2 && connection_get_host() == CONNECTION_HOST_BLUETOOTH && (pending_command == NO_COMMAND || phase == FAILED || phase == OFF)) md770_radio_select(deferred_slot, false);
    if (phase == FAILED || phase == OFF || stopped || radio_suspended) return;
    if (phase == RESET_HIGH || phase == RESET_LOW) {
        if (timer_elapsed32(phase_started) < (phase == RESET_HIGH ? 1000 : 1500)) return;
        if (phase == RESET_HIGH) {
            if (pending_command == COMMAND_STOP) {
                phase = OFF;
                CKCU->APBCCR0 &= ~CKCU_APBCCR0_SPI0EN;
                return;
            }
            gpio_write_pin_low(D1);
            phase         = RESET_LOW;
            phase_started = timer_read32();
        } else if (!startup_attempts) {
            deferred_slot   = 0;
            radio_state     = 7;
            pending_command = COMMAND_STOP;
            phase           = FAILED;
            SPI0->IER       = 0;
        } else {
            --startup_attempts;
            rearm();
        }
        return;
    }
    uint8_t length   = 0;
    bool    complete = false, error;
    chSysLock();
    error = failed || (SPI0->SR & 0xf8);
    if (!error) {
        service_fifo();
        error = failed || (SPI0->SR & 0xf8);
    }
    if (!error && selected && !(SPI0->SR & 0x100)) {
        service_fifo();
        error |= failed || (SPI0->SR & 0xf8);
        length   = rx_length;
        complete = true;
    }
    chSysUnlock();
    if (error) {
        rearm();
        return;
    }
    if (complete) {
        bool transmitting = phase == TRANSMITTING;
        bool collision    = parsed;
        gpio_write_pin_low(C14);
        rearm();
        if (transmitting && !collision) {
            if (length && radio_state < 2) radio_state = 2;
            complete_command(length);
        }
    }
    if (phase == TRANSMITTING) {
        if (!parsed && timer_elapsed32(phase_started) >= 2000) pulse_radio();
        return;
    }
    if (selected) return;
    if (deferred_slot && !held_slot && all_keys_up() && radio_state >= 2 && connection_get_host() == CONNECTION_HOST_BLUETOOTH && (pending_command == NO_COMMAND || phase == FAILED || phase == OFF)) md770_radio_select(deferred_slot, false);
    if ((int32_t)(timer_read32() - ready_at) >= 0) build_packet();
}

void __attribute__((noinline, noclone)) bluetooth_task(void) {
    if (stopped) return;
    md770_indicators_task();
    uint8_t keys = 0;
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        if (row != 7) keys += __builtin_popcount(matrix_get_row(row));
    }
    if (held_slot && held_armed && keys == 2 && md770_radio_pairing_chord() && (matrix_get_row(held_key.row) & ((matrix_row_t)1 << held_key.col)) && timer_elapsed32(held_started) >= 3000) {
        uint8_t slot = held_slot;
        held_slot    = 0;
        md770_radio_select(slot, true);
    }
    radio_task();
}

void md770_radio_shutdown(void) {
    deferred_slot = held_slot = 0;
    pending_command           = COMMAND_STOP;
    chSysLock();
    SPI0->IER = 0;
    gpio_write_pin_low(C14);
    chSysUnlock();
    stopped = true;
}
