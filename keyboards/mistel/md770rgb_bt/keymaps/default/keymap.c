// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "keymap.h"
#ifdef MAGIC_ENABLE
#    include "process_magic.h"
#endif

#ifdef DIP_SWITCH_ENABLE
enum dip_switches {
    DIP_MAC          = 1 << 0,
    DIP_CONTROL_CAPS = 1 << 1,
    DIP_ALT_GUI      = 1 << 2,
    DIP_CAPS_FN      = 1 << 3,
};

static uint8_t dip_state;

static void apply_dip_modifiers(void) {
    bool mac                            = dip_state & DIP_MAC;
    keymap_config.swap_control_capslock = (dip_state & DIP_CONTROL_CAPS) != 0;
    keymap_config.swap_lalt_lgui        = mac || (dip_state & DIP_ALT_GUI);
    keymap_config.swap_ralt_rgui        = mac;
}

bool dip_switch_update_mask_user(uint32_t state) {
    bool mac               = state & DIP_MAC;
    bool modifiers_changed = keymap_config.swap_control_capslock != ((state & DIP_CONTROL_CAPS) != 0) || keymap_config.swap_lalt_lgui != (mac || (state & DIP_ALT_GUI)) || keymap_config.swap_ralt_rgui != mac;
    if (dip_state != state || modifiers_changed) {
        clear_keyboard();
    }
    dip_state = state;
    apply_dip_modifiers();
    layer_state_t next = layer_state & ~((1UL << MAC) | (1UL << CAPS_FN));
    if (state & DIP_MAC) {
        next |= 1UL << MAC;
    }
    if ((state & DIP_CAPS_FN) && !(state & DIP_CONTROL_CAPS)) {
        next |= 1UL << CAPS_FN;
    }
    layer_state_set(next);
    return true;
}
#endif

#ifdef RGB_MATRIX_ENABLE
static hsv_t rgb_to_hsv(rgb_t colour) {
    uint8_t maximum = MAX(colour.r, MAX(colour.g, colour.b));
    uint8_t minimum = MIN(colour.r, MIN(colour.g, colour.b));
    uint8_t range   = maximum - minimum;
    hsv_t   hsv     = {0, 0, maximum};
    if (!range) {
        return hsv;
    }
    hsv.s = (uint16_t)range * UINT8_MAX / maximum;
    int16_t hue;
    if (maximum == colour.r) {
        hue = 43 * ((int16_t)colour.g - colour.b) / range;
    } else if (maximum == colour.g) {
        hue = 85 + 43 * ((int16_t)colour.b - colour.r) / range;
    } else {
        hue = 171 + 43 * ((int16_t)colour.r - colour.g) / range;
    }
    hsv.h = hue;
    return hsv;
}

static void set_solid_colour(hsv_t colour) {
    rgb_matrix_enable();
    rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_sethsv(colour.h, colour.s, colour.v);
}

static void adjust_rgb_channel(uint16_t keycode) {
    rgb_t    colour  = hsv_to_rgb_nocie(rgb_matrix_get_hsv());
    uint8_t *channel = keycode == MD770_RGB_RED ? &colour.r : keycode == MD770_RGB_GREEN ? &colour.g : &colour.b;
    uint16_t next    = *channel + 16;
    *channel         = next > RGB_MATRIX_MAXIMUM_BRIGHTNESS ? 0 : next;
    set_solid_colour(rgb_to_hsv(colour));
}

static void next_palette_colour(void) {
    static const hsv_t palette[] = {{HSV_WHITE}, {HSV_RED}, {HSV_ORANGE}, {HSV_YELLOW}, {HSV_GREEN}, {HSV_CYAN}, {HSV_BLUE}, {HSV_PURPLE}};
    static uint8_t     next;
    hsv_t              colour = palette[next];
    next                      = (next + 1) % ARRAY_SIZE(palette);
    colour.v                  = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    set_solid_colour(colour);
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#if defined(MAGIC_ENABLE) && defined(DIP_SWITCH_ENABLE)
    if (IS_MAGIC_KEYCODE(keycode)) {
        bool result = process_magic(keycode, record);
        apply_dip_modifiers();
        return result;
    }
#endif
    switch (keycode) {
        case MD770_NEXT_LAYOUT:
            if (record->event.pressed) {
                set_single_persistent_default_layer((get_highest_layer(default_layer_state) + 1) % 3);
            }
            return false;
        case MD770_RGB_PALETTE:
        case MD770_RGB_RED:
        case MD770_RGB_GREEN:
        case MD770_RGB_BLUE:
        case MD770_RGB_NEXT:
#ifdef RGB_MATRIX_ENABLE
            if (record->event.pressed) {
                if (keycode == MD770_RGB_PALETTE) {
                    next_palette_colour();
                } else if (keycode == MD770_RGB_NEXT) {
                    if (rgb_matrix_is_enabled()) {
                        rgb_matrix_step();
                    } else {
                        rgb_matrix_enable();
                    }
                } else {
                    adjust_rgb_channel(keycode);
                }
            }
#endif
            return false;
    }
    return true;
}
