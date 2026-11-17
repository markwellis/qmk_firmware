// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define CH_CFG_ST_FREQUENCY 2000
#define CH_CFG_ST_TIMEDELTA 0
#define CH_CFG_TIME_QUANTUM 20
#include_next <chconf.h>

// The retained IAP can leave the watchdog running during flash initialisation.
#undef CH_CFG_SYSTEM_TICK_HOOK
#ifndef _FROM_ASM_
void md770_power_tick(void);
#endif
#define CH_CFG_SYSTEM_TICK_HOOK() md770_power_tick()
