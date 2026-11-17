// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"
#include "connection.h"
#include "usb_device_state.h"
#include "suspend.h"

const uint32_t           md770_system_tick_frequency = CH_CFG_ST_FREQUENCY;
static volatile uint32_t activity;
static volatile bool     power_idle;
static bool              external, supply_known, usb_suspended;
#ifndef MD770_LEFT
static volatile uint16_t lvd_remaining;
static bool              battery_low;
#endif

void __attribute__((noinline, noclone)) md770_power_tick(void) {
    if (md770_power_sleeping()) return;
    *(volatile uint32_t *)&WDT->CR = MD770_WATCHDOG_RELOAD;
#ifndef MD770_LEFT
    md770_radio_tick();
    if (lvd_remaining) {
        if ((PWRCU->LVDCSR & 0x90000U) == 0x90000U)
            --lvd_remaining;
        else
            lvd_remaining = 50;
    }
#endif
}

void md770_power_init(void) {
    gpio_set_pin_input_high(B12);
#ifndef MD770_LEFT
    gpio_set_pin_input_high(C13);
#endif
    activity = timer_read32();
}

void md770_power_activity(bool local) {
    chSysLock();
    if (!power_idle || local) {
        activity   = timer_read32();
        power_idle = false;
    }
    chSysUnlock();
}

void md770_power_radio_activity(void) {
    activity = timer_read32();
}

void md770_power_keep_awake(uint32_t minimum) {
    chSysLock();
    uint32_t elapsed = timer_elapsed32(activity);
    if (elapsed >= 600000 || 600000 - elapsed < minimum) activity = timer_read32() - (600000 - minimum);
    chSysUnlock();
}

void md770_power_limit_awake(uint32_t maximum) {
    chSysLock();
    uint32_t elapsed = timer_elapsed32(activity);
    if (elapsed < 600000 && 600000 - elapsed > maximum) activity = timer_read32() - (600000 - maximum);
    chSysUnlock();
}

bool md770_power_idle(void) {
    return power_idle;
}

bool md770_battery_low(void) {
#ifndef MD770_LEFT
    return !external && battery_low;
#else
    return false;
#endif
}

bool md770_charge_indicated(void) {
    return external;
}

bool md770_power_rgb_allowed(void) {
    if (usb_suspended) return false;
#ifdef MD770_LEFT
    return md770_link_rgb_active();
#else
    return !md770_battery_low();
#endif
}

bool md770_power_usb_suspended(void) {
    return usb_suspended;
}

void keyboard_post_init_kb(void) {
    supply_known = false;
#ifdef MD770_LEFT
    connection_set_host_noeeprom(md770_link_connected() ? CONNECTION_HOST_NONE : CONNECTION_HOST_USB);
#endif
    keyboard_post_init_user();
}

void connection_host_changed_kb(connection_host_t host) {
    bool suspended     = host == CONNECTION_HOST_USB && usb_device_state_get_configure_state() == USB_DEVICE_STATE_SUSPEND;
    bool was_suspended = usb_suspended;
    usb_suspended      = suspended;
    if (suspended && !was_suspended) suspend_power_down_quantum();
    if (!suspended && was_suspended) suspend_wakeup_init_quantum();
#ifndef MD770_LEFT
    if (host == CONNECTION_HOST_USB) md770_radio_release_keep_awake();
#endif
    host_task();
}

#ifndef MD770_LEFT
void host_disconnect_active_driver_kb(connection_host_t host) {
    (void)host;
    md770_playback_cancel_physical();
    if (host == CONNECTION_HOST_BLUETOOTH) md770_radio_release_reports();
}
#endif

void notify_usb_device_state_change_kb(struct usb_device_state state) {
    usb_suspended = state.configure_state == USB_DEVICE_STATE_SUSPEND && connection_get_host() == CONNECTION_HOST_USB;
    if (usb_suspended) suspend_power_down_quantum();
#ifndef MD770_LEFT
    if (state.configure_state != USB_DEVICE_STATE_CONFIGURED && connection_get_host() == CONNECTION_HOST_USB) md770_playback_cancel_physical();
#endif
    notify_usb_device_state_change_user(state);
}

#ifndef MD770_LEFT
bool md770_power_usb_ready(void) {
    return gpio_read_pin(B12) && USBD1.state == USB_ACTIVE && USBD1.configuration != 0;
}

static void supply_task(bool powered) {
    if (powered && !external && !md770_radio_usb_allowed()) return;
    if (!supply_known || external != powered) {
        external     = powered;
        supply_known = true;
        battery_low  = false;
        chSysLock();
        PWRCU->LVDCSR = (PWRCU->LVDCSR & ~0xff0000U) | (powered ? 0 : 0x410000U);
        lvd_remaining = powered ? 0 : 50;
        chSysUnlock();
        md770_power_activity(true);
    }
    if (!external && !lvd_remaining) battery_low = true;
}
#endif

void md770_power_usb_wake(void) {
    if (connection_get_host() == CONNECTION_HOST_USB && USBD1.state == USB_SUSPENDED && (USBD1.status & (1U << 1))) usbWakeupHost(&USBD1);
}

void md770_power_task(void) {
    bool powered = gpio_read_pin(B12);
    if (powered && power_idle) md770_power_activity(true);
#ifndef MD770_LEFT
    supply_task(powered);
    chSysLock();
    if (!powered && timer_elapsed32(activity) >= 600000) power_idle = true;
    chSysUnlock();
#else
    if (!supply_known || powered != external) {
        external     = powered;
        supply_known = true;
        md770_power_activity(true);
    }
    chSysLock();
    if (!powered && timer_elapsed32(activity) >= 300) power_idle = true;
    chSysUnlock();
#endif
    md770_power_service();
}
