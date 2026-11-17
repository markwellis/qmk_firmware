// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "keymap.h"

#ifdef BLUETOOTH_ENABLE
bool md770_radio_pairing_chord(void) {
    return layer_state_is(PN);
}
#endif
