// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define CH_CFG_ST_FREQUENCY 1000
#define CH_CFG_ST_TIMEDELTA 0
#define CH_CFG_TIME_QUANTUM 20
#include_next <chconf.h>

// The retained IAP can leave the watchdog running during flash initialisation.
#undef CH_CFG_SYSTEM_TICK_HOOK
#define CH_CFG_SYSTEM_TICK_HOOK() (*(volatile uint32_t *)0x40068000U = 0x5fa00001U)
