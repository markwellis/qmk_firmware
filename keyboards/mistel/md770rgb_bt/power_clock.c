// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "md770rgb_bt.h"

static volatile bool sleeping;

bool md770_power_sleeping(void) {
    return sleeping;
}

static void power_pin_mode(pin_t pin, uint32_t mode) {
    if (PAL_PORT(pin)->DRVR & (1U << PAL_PAD(pin))) mode |= PAL_HT32_MODE_DRV;
    palSetLineMode(pin, mode);
}

static void gpio_sleep(bool down) {
#ifdef MD770_LEFT
    static const pin_t led_pins[] = {C4, C5, C8};
    for (uint8_t i = 0; i < ARRAY_SIZE(led_pins); ++i) {
        if (!down) gpio_write_pin_high(led_pins[i]);
        power_pin_mode(led_pins[i], PAL_MODE_HT32_AF(0) | (down ? 0 : PAL_MODE_OUTPUT_PUSHPULL | PAL_MODE_INPUT));
    }
    power_pin_mode(B8, PAL_MODE_HT32_AF(0) | PAL_MODE_INPUT_PULLDOWN);
    if (down) {
        AFIO->ESSR[1] = (AFIO->ESSR[1] & ~0xfU) | 1U;
        EXTI->WAKUPCR |= 0x100;
    }
#else
    static const pin_t led_pins[] = {C4, C5, C8, A14, D3};
    for (uint8_t i = 0; i < ARRAY_SIZE(led_pins); ++i) {
        if (!down) gpio_write_pin_high(led_pins[i]);
        power_pin_mode(led_pins[i], PAL_MODE_HT32_AF(down ? 0 : 4) | (down ? 0 : PAL_MODE_OUTPUT_PUSHPULL));
    }
    power_pin_mode(C13, PAL_MODE_HT32_AF(0) | (down ? 0 : PAL_MODE_INPUT_PULLUP));
    if (!down) gpio_write_pin_high(B11);
    power_pin_mode(B11, PAL_MODE_HT32_AF(1) | (down ? 0 : PAL_MODE_OUTPUT_PUSHPULL | PAL_MODE_INPUT));
    power_pin_mode(B8, PAL_MODE_HT32_AF(0) | (down ? 0 : PAL_MODE_INPUT_PULLUP));
#endif
#ifndef MD770_LEFT
    if (!down) gpio_write_pin_high(C10);
#endif
    power_pin_mode(C10, PAL_MODE_HT32_AF(down ? 0 : 4) | (down ? 0 : PAL_MODE_OUTPUT_PUSHPULL));
}

static uint32_t deep_sleep(void) {
    uint32_t wake_mask    = EXTI->WAKUPCR;
    uint32_t backup_clock = CKCU->APBCCR1 & CKCU_APBCCR1_BKPREN;
    SysTick->CTRL         = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    CKCU->APBCCR1 |= CKCU_APBCCR1_BKPREN;
    *(volatile uint32_t *)&WDT->CR = MD770_WATCHDOG_RELOAD;
    palSetLineMode(B12, PAL_MODE_HT32_AF(15) | PAL_MODE_INPUT_PULLUP);
    PWRCU->BAKCR |= 1U << 8;
    wait_us(1);
    (void)PWRCU->BAKSR;
    EXTI->WAKUPFLG = 0xffff;
    wait_us(1000);
    EXTI->WAKUPPOLR ^= EXTI->WAKUPFLG;
    EXTI->WAKUPFLG = 0xffff;
    PWRCU->BAKCR &= ~((1U << 7) | (1U << 3));
    if (!backup_clock) CKCU->APBCCR1 &= ~CKCU_APBCCR1_BKPREN;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __SEV();
    __WFE();
    __WFE();
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    CKCU->APBCCR1 |= CKCU_APBCCR1_BKPREN;
    PWRCU->BAKCR &= ~(1U << 8);
    palSetLineMode(B12, PAL_MODE_HT32_AF(1) | PAL_MODE_INPUT_PULLUP);
    (void)PWRCU->BAKSR;
    SysTick->CTRL   = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk;
    SysTick->VAL    = 0;
    EXTI->WAKUPCR   = wake_mask;
    uint32_t events = EXTI->WAKUPFLG;
    if (!backup_clock) CKCU->APBCCR1 &= ~CKCU_APBCCR1_BKPREN;
    return events;
}

static void idle_loop(void) {
    enum idle_phase { IDLE_INITIAL_SCAN, IDLE_RESTORE_SCAN, IDLE_SETTLING };
    enum { IDLE_SETTLE_SCANS = 19 };
    enum idle_phase phase     = IDLE_INITIAL_SCAN;
    uint8_t         remaining = 0;
    md770_scan_idle_begin();
    md770_scan_prepare_wake();
    while (!gpio_read_pin(B12)) {
        if (phase == IDLE_RESTORE_SCAN) {
            remaining = IDLE_SETTLE_SCANS;
            phase     = IDLE_SETTLING;
            md770_scan_finish_wake();
        } else {
            bool held;
            bool changed = md770_scan_idle_changed(&held);
            if (changed) {
#ifndef MD770_LEFT
                if (phase != IDLE_INITIAL_SCAN || md770_radio_state() <= 4) break;
                md770_scan_idle_normalise();
                phase = IDLE_RESTORE_SCAN;
#else
                break;
#endif
            } else if (held) {
                remaining = IDLE_SETTLE_SCANS;
                phase     = IDLE_SETTLING;
            } else if (phase == IDLE_INITIAL_SCAN || --remaining == 0)
                phase = IDLE_RESTORE_SCAN;
            if (phase == IDLE_RESTORE_SCAN) md770_scan_prepare_wake();
        }
        if (phase == IDLE_SETTLING) {
            *(volatile uint32_t *)&WDT->CR = MD770_WATCHDOG_RELOAD;
            wait_ms(2);
        } else {
            uint32_t events = deep_sleep();
#ifdef MD770_LEFT
            if (events & ~0xffU) break;
#else
            if (events & ~0x1feU) break;
#endif
        }
    }
}

void __attribute__((noinline, noclone)) md770_power_service(void) {
    static bool initialised;
    if (!initialised) {
#ifdef MD770_LEFT
        GPIOC->DRVR |= (1U << 8) | (1U << 10);
#endif
        gpio_sleep(false);
        initialised = true;
    }
    md770_rgb_sleep(!md770_power_rgb_allowed());
    if (!md770_power_idle()) return;
    chSysLock();
    bool ready = md770_link_can_sleep();
    chSysUnlock();
    if (!ready) return;
#ifndef MD770_LEFT
    md770_playback_cancel_physical();
    clear_keyboard();
    if (!md770_storage_sleep()) return;
#endif
    md770_scan_pause();
    md770_rgb_sleep(true);
    sleeping = true;
    md770_link_sleep(true);
#ifndef MD770_LEFT
    chSysLock();
    md770_radio_power_downI();
    chSysUnlock();
    md770_indicators_sleep(true);
#endif
    gpio_sleep(true);
    idle_loop();
#ifndef MD770_LEFT
    chSysLock();
    md770_radio_power_upI();
    chSysUnlock();
#endif
    gpio_sleep(false);
    md770_link_sleep(false);
#ifndef MD770_LEFT
    if (!md770_storage_wake()) chSysHalt("flash");
#endif
    sleeping = false;
    md770_power_activity(true);
#ifndef MD770_LEFT
    clear_keyboard();
    md770_indicators_sleep(false);
#endif
    md770_scan_set_idle(false);
    md770_scan_finish_wake();
    md770_rgb_sleep(!md770_power_rgb_allowed());
}
