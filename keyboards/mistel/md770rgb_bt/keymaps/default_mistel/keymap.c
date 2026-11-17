// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "keymap.h"
#include "programming.h"
#include <string.h>
#ifdef MAGIC_ENABLE
#    include "process_magic.h"
#endif
#ifdef DYNAMIC_KEYMAP_ENABLE
#    include "dynamic_keymap.h"
#    include "keymap_introspection.h"
#endif

#define LAYOUT_MASK ((1UL << BASE) | (1UL << COLEMAK) | (1UL << DVORAK))
#define PROFILE_MASK ((1UL << PROFILE1) | (1UL << PROFILE2) | (1UL << PROFILE3))

static uint8_t         reset_alts;
static uint32_t        reset_started;
static bool            reset_pending;
static uint32_t        profile_reset_started;
static bool            profile_reset_pending;
static md770_profile_t profile_to_reset = MD770_PROFILE_NONE;
static md770_profile_t active_profile;
static uint32_t        feedback_started;
static bool            feedback_active;
#ifdef DIP_SWITCH_ENABLE
static void apply_dip_layers(layer_state_t state);
#endif
#ifdef RGB_MATRIX_ENABLE
hsv_t               md770_custom_colours[RGB_MATRIX_LED_COUNT];
static bool         rgb_editing;
static matrix_row_t rgb_suppressed[MATRIX_ROWS];
#endif

layer_state_t layer_state_set_user(layer_state_t state) {
    return md770_programming_layers(state, active_profile);
}

static void update_layer_indicator(layer_state_t state) {
    static const uint8_t colours[MD770_PROFILE_NONE] = {0, 1, 2, 4};
    uint8_t              colour                      = colours[md770_profile_for_state(state)];
#ifdef RGB_MATRIX_ENABLE
    if (rgb_editing) {
        colour = 7;
    }
#endif
    if (feedback_active) {
        colour = timer_elapsed32(feedback_started) / 100 % 2 ? 0 : 7;
    }
    colour                         = md770_programming_indicator(colour);
    static uint8_t previous_colour = UINT8_MAX;
    if (colour != previous_colour) {
        previous_colour = colour;
        md770_set_layer_led(colour);
    }
}

layer_state_t default_layer_state_set_user(layer_state_t state) {
    active_profile = md770_profile_for_state(state);
#ifdef DIP_SWITCH_ENABLE
    apply_dip_layers(state);
#else
    layer_state_set(layer_state);
#endif
    update_layer_indicator(state);
    return state;
}

static void persist_layers(layer_state_t state) {
    md770_playback_cancel();
    clear_keyboard();
    default_layer_set(state);
    eeconfig_update_default_layer(state);
}

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
        md770_playback_cancel();
        clear_keyboard();
    }
    dip_state = state;
    apply_dip_modifiers();
    apply_dip_layers(default_layer_state);
    return true;
}

static void apply_dip_layers(layer_state_t state) {
    layer_state_t next = layer_state & ~((1UL << MAC) | (1UL << CAPS_FN));
    if (dip_state & DIP_MAC) {
        next |= 1UL << MAC;
    }
    if ((dip_state & DIP_CAPS_FN) && !(dip_state & DIP_CONTROL_CAPS) && !md770_programming_blocks_fn_swap(state)) {
        next |= 1UL << CAPS_FN;
    }
    layer_state_set(next);
}
#endif

#if defined(DYNAMIC_KEYMAP_ENABLE) && !defined(VIA_ENABLE)
void eeconfig_init_user(void) {
    dynamic_keymap_macro_reset();
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
    if (!rgb_editing) {
        rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);
    }
    rgb_matrix_sethsv(colour.h, colour.s, colour.v);
}

static void adjust_rgb_channel(uint16_t keycode) {
    rgb_t    colour = hsv_to_rgb_nocie(rgb_matrix_get_hsv());
    uint8_t *channel;
    switch (keycode) {
        case MD770_RGB_RED:
            channel = &colour.r;
            break;
        case MD770_RGB_GREEN:
            channel = &colour.g;
            break;
        default:
            channel = &colour.b;
            break;
    }
    uint16_t next = *channel + 16;
    *channel      = next > RGB_MATRIX_MAXIMUM_BRIGHTNESS ? 0 : next;
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

void eeconfig_init_user_datablock(void) {
    memset(md770_custom_colours, 0, sizeof(md770_custom_colours));
    eeconfig_update_user_datablock(md770_custom_colours, 0, sizeof(md770_custom_colours));
}

static void finish_rgb_edit(void) {
    if (rgb_editing) {
        eeconfig_update_user_datablock(md770_custom_colours, 0, sizeof(md770_custom_colours));
        rgb_editing = false;
        update_layer_indicator(default_layer_state);
    }
}
#endif

void keyboard_post_init_user(void) {
    layer_state_set(layer_state);
#ifdef RGB_MATRIX_ENABLE
    eeconfig_read_user_datablock(md770_custom_colours, 0, sizeof(md770_custom_colours));
#endif
}

__attribute__((weak)) void housekeeping_task_keymap(void) {}

void housekeeping_task_user(void) {
    housekeeping_task_keymap();
#ifdef RGB_MATRIX_ENABLE
    if (rgb_editing && (!rgb_matrix_is_enabled() || rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_MD770_CUSTOM)) {
        finish_rgb_edit();
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (!rgb_editing)
#endif
    {
        md770_programming_task();
    }
    if (reset_pending && timer_elapsed32(reset_started) >= 1000) {
        reset_pending = false;
        eeconfig_disable();
        soft_reset_keyboard();
    }
    if (profile_reset_pending && timer_elapsed32(profile_reset_started) >= 1000) {
        profile_reset_pending = false;
        profile_to_reset      = md770_profile_for_state(default_layer_state);
    }
    if (profile_to_reset != MD770_PROFILE_NONE) {
        // Dynamic keycodes must remain available until their releases are
        // processed.
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            if (matrix_get_row(row)) {
                return;
            }
        }
        md770_playback_cancel();
        clear_keyboard();
#ifdef DYNAMIC_KEYMAP_ENABLE
        for (md770_bank_t bank = MD770_BANK_MAIN; bank < MD770_BANK_NONE; ++bank) {
            uint8_t layer = md770_profile_layer(profile_to_reset, bank);
            for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
                for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                    dynamic_keymap_set_keycode(layer, row, col, keycode_at_keymap_location_raw(layer, row, col));
                }
            }
        }
#endif
        default_layer_set(default_layer_state);
        profile_to_reset = MD770_PROFILE_NONE;
        feedback_active  = true;
        feedback_started = timer_read32();
        update_layer_indicator(default_layer_state);
    }
    if (feedback_active && timer_elapsed32(feedback_started) >= 1000) {
        feedback_active = false;
    }
    update_layer_indicator(default_layer_state);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    bool physical = IS_KEYEVENT(record->event) && record->event.key.row < MATRIX_ROWS && record->event.key.col < MATRIX_COLS;
#ifdef RGB_MATRIX_ENABLE
    matrix_row_t key_bit = physical ? (matrix_row_t)1U << record->event.key.col : 0;
#endif
    uint8_t alt = 0;
    if (record->event.key.row == 12 && record->event.key.col == 4) {
        alt = 1;
    } else if (record->event.key.row == 1 && record->event.key.col == 4) {
        alt = 2;
    }
    if (physical && alt) {
        reset_alts    = record->event.pressed ? reset_alts | alt : reset_alts & ~alt;
        reset_pending = reset_alts == 3;
        reset_started = timer_read32();
    }
#ifdef RGB_MATRIX_ENABLE
    if (physical && !record->event.pressed && (rgb_suppressed[record->event.key.row] & key_bit)) {
        rgb_suppressed[record->event.key.row] &= ~key_bit;
        return false;
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (!rgb_editing)
#endif
    {
        if (!md770_programming_process(keycode, record)) {
            return false;
        }
    }
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
                uint8_t layout = (get_highest_layer(default_layer_state & LAYOUT_MASK) + 1) % 3;
                persist_layers((default_layer_state & PROFILE_MASK) | (1UL << layout));
            }
            return false;
        case MD770_PROFILE0:
        case MD770_PROFILE1:
        case MD770_PROFILE2:
        case MD770_PROFILE3:
            if (record->event.pressed) {
                md770_profile_t profile = keycode - MD770_PROFILE0;
                layer_state_t   layers  = default_layer_state & LAYOUT_MASK;
                if (profile != MD770_PROFILE_DEFAULT) {
                    layers |= 1UL << md770_profile_layer(profile, MD770_BANK_MAIN);
                }
                persist_layers(layers);
            }
            return false;
        case MD770_RESET_PROFILE:
            profile_reset_pending = record->event.pressed;
            profile_reset_started = timer_read32();
            return false;
        case MD770_RGB_EDIT:
        case MD770_RGB_CUSTOM:
#ifdef RGB_MATRIX_ENABLE
            if (record->event.pressed) {
                if (rgb_editing) {
                    finish_rgb_edit();
                } else if (keycode == MD770_RGB_EDIT) {
                    md770_playback_cancel();
                    clear_keyboard();
                    rgb_editing = true;
                    update_layer_indicator(default_layer_state);
                }
                rgb_matrix_enable();
                rgb_matrix_mode(RGB_MATRIX_CUSTOM_MD770_CUSTOM);
            }
#endif
            return false;
        case RM_OFF:
#ifdef RGB_MATRIX_ENABLE
            if (record->event.pressed) {
                finish_rgb_edit();
            }
#endif
            return true;
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
                    finish_rgb_edit();
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
#ifdef RGB_MATRIX_ENABLE
    if (physical && rgb_editing && record->event.pressed && !IS_QK_MOMENTARY(keycode) && !IS_RGB_MATRIX_KEYCODE(keycode) && keycode != QK_BOOT) {
        rgb_suppressed[record->event.key.row] |= key_bit;
        if (!layer_state_is(PN)) {
            uint8_t led = g_led_config.matrix_co[record->event.key.row][record->event.key.col];
            if (led < RGB_MATRIX_LED_COUNT) {
                hsv_t  colour = rgb_matrix_get_hsv();
                hsv_t *saved  = &md770_custom_colours[led];
                colour.v      = saved->v && saved->h == colour.h && saved->s == colour.s ? 0 : UINT8_MAX;
                *saved        = colour;
            }
        }
        return false;
    }
#endif
    return true;
}

#ifdef BLUETOOTH_ENABLE
bool md770_radio_pairing_chord(void) {
    return layer_state_is(PN);
}
#endif
