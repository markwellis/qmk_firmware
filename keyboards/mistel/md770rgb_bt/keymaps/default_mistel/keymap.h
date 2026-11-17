// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include QMK_KEYBOARD_H

enum custom_keycodes {
    MD770_NEXT_LAYOUT = QK_KB,
    MD770_RGB_PALETTE,
    MD770_RGB_RED,
    MD770_RGB_GREEN,
    MD770_RGB_BLUE,
    MD770_RGB_NEXT,
    MD770_PROFILE0,
    MD770_PROFILE1,
    MD770_PROFILE2,
    MD770_PROFILE3,
    MD770_RESET_PROFILE,
    MD770_RGB_EDIT,
    MD770_RGB_CUSTOM,
};

enum layer_names { BASE, COLEMAK, DVORAK, MAC, CAPS_FN, PROFILE1, PROFILE2, PROFILE3, FN, PN, FN1, FN2, FN3, PN1, PN2, PN3, DEFAULT_PROFILE, DEFAULT_FN, DEFAULT_PN };

typedef enum { MD770_PROFILE_DEFAULT, MD770_PROFILE_ONE, MD770_PROFILE_TWO, MD770_PROFILE_THREE, MD770_PROFILE_NONE } md770_profile_t;
typedef enum { MD770_BANK_MAIN, MD770_BANK_FN, MD770_BANK_PN, MD770_BANK_NONE } md770_bank_t;

static inline uint8_t md770_profile_layer(md770_profile_t profile, md770_bank_t bank) {
    static const uint8_t layers[MD770_PROFILE_NONE][MD770_BANK_NONE] = {
        {DEFAULT_PROFILE, DEFAULT_FN, DEFAULT_PN},
        {PROFILE1, FN1, PN1},
        {PROFILE2, FN2, PN2},
        {PROFILE3, FN3, PN3},
    };
    return layers[profile][bank];
}

static inline md770_profile_t md770_profile_for_state(layer_state_t state) {
    switch (get_highest_layer(state & ((1UL << PROFILE1) | (1UL << PROFILE2) | (1UL << PROFILE3)))) {
        case PROFILE1:
            return MD770_PROFILE_ONE;
        case PROFILE2:
            return MD770_PROFILE_TWO;
        case PROFILE3:
            return MD770_PROFILE_THREE;
        default:
            return MD770_PROFILE_DEFAULT;
    }
}

static inline md770_bank_t md770_bank_for_state(layer_state_t state) {
    if (state & (1UL << PN)) return MD770_BANK_PN;
    if (state & (1UL << FN)) return MD770_BANK_FN;
    return MD770_BANK_MAIN;
}
