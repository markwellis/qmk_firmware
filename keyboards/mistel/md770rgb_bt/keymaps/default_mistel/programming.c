// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "programming.h"
#ifdef BLUETOOTH_ENABLE
#    include "connection.h"
#endif

#ifdef DYNAMIC_KEYMAP_ENABLE
#    include "dynamic_keymap.h"
#    include "keycode_config.h"
#    include "keymap.h"
#    include "keymap_introspection.h"
#    include "send_string.h"
#    include <string.h>

#    ifndef MD770_MACRO_RECORD_EVENTS
#        define MD770_MACRO_RECORD_EVENTS 200
#    endif

_Static_assert(MD770_MACRO_RECORD_EVENTS >= 2 && MD770_MACRO_RECORD_EVENTS <= UINT16_MAX / 12, "Invalid MD770 macro event count");
_Static_assert(DYNAMIC_KEYMAP_MACRO_COUNT > 0 && DYNAMIC_KEYMAP_MACRO_COUNT <= QK_MACRO_MAX - QK_MACRO + 1, "Invalid macro count");

enum programming_state { IDLE, TARGET, RECORDING, MOVE_FN, MOVE_PN, SAVE_RECORDING, SAVE_MOVE };
enum playback_state { PLAYBACK_IDLE, PLAYBACK_RUNNING, PLAYBACK_RELEASING };
typedef struct {
    uint16_t delay;
    uint8_t  code;
    bool     down;
} recorded_event_t;
static enum programming_state state;
static matrix_row_t           suppressed[MATRIX_ROWS];
static uint8_t                recorded_down[MATRIX_ROWS][MATRIX_COLS];
static recorded_event_t       recording[MD770_MACRO_RECORD_EVENTS];
static uint16_t               recording_size, first_code, delay_ticks;
static uint8_t                delay_keys;
static uint8_t                presses, held, target_layer;
static md770_profile_t        profile;
static md770_bank_t           move_bank = MD770_BANK_NONE;
static keypos_t               destination;
static uint32_t               blink_started, move_started;
static bool                   overflow;
static enum playback_state    playback_state;
static uint16_t               playback_offset, playback_end;
static keypos_t               playback_trigger;
static uint32_t               playback_started, playback_wait;
static uint8_t                playback_keys[32], playback_mods;
typedef struct {
    uint8_t  code;
    bool     down;
    uint16_t delay;
} playback_event_t;
static playback_event_t playback_sequence[10];
static uint8_t          playback_steps, playback_step;
static bool             playback_pending;
static uint8_t          playback_pending_slot;
static keypos_t         playback_pending_trigger;

layer_state_t md770_programming_layers(layer_state_t layers, md770_profile_t selected) {
    layers &= ~((7UL << FN1) | (7UL << PN1) | (7UL << DEFAULT_PROFILE));
    md770_bank_t bank = md770_bank_for_state(layers);
    if (selected == MD770_PROFILE_DEFAULT) {
        layers |= 1UL << DEFAULT_PROFILE;
    }
    if (bank != MD770_BANK_MAIN) {
        layers |= 1UL << md770_profile_layer(selected, bank);
    }
    return layers;
}

static uint16_t profile_keycode(md770_profile_t selected, keypos_t key) {
    uint8_t  layer = md770_profile_layer(selected, MD770_BANK_MAIN);
    uint16_t code  = dynamic_keymap_get_keycode(layer, key.row, key.col);
    if (code == KC_TRNS) {
        uint8_t base = get_highest_layer(default_layer_state & ((1UL << BASE) | (1UL << COLEMAK) | (1UL << DVORAK)));
        code         = dynamic_keymap_get_keycode(base, key.row, key.col);
    }
    return code;
}

static uint16_t source_keycode(keypos_t key) {
    layer_state_t layers = (layer_state & ((1UL << MAC) | (1UL << CAPS_FN) | (1UL << FN) | (1UL << PN))) | (default_layer_state & ((1UL << BASE) | (1UL << COLEMAK) | (1UL << DVORAK)));
    for (int layer = PN; layer >= BASE; --layer) {
        uint16_t code = keycode_at_keymap_location_raw(layer, key.row, key.col);
        if ((layers & (1UL << layer)) && code != KC_TRNS) {
            if (code == MO(FN) && profile_keycode(profile, key) != MO(FN)) return KC_RGUI;
            if (code == MO(PN) && profile_keycode(profile, key) != MO(PN)) return KC_APP;
            return code;
        }
    }
    return KC_NO;
}

bool md770_programming_blocks_fn_swap(layer_state_t layers) {
    md770_profile_t selected = md770_profile_for_state(layers);
    uint8_t         layer    = md770_profile_layer(selected, MD770_BANK_MAIN);
    uint8_t         base     = get_highest_layer(layers & ((1UL << BASE) | (1UL << COLEMAK) | (1UL << DVORAK)));
    if (dynamic_keymap_get_keycode(layer, 13, 3) != keycode_at_keymap_location_raw(layer, 13, 3)) {
        return true;
    }
    if (dynamic_keymap_get_keycode(base, 13, 3) != keycode_at_keymap_location_raw(base, 13, 3)) return true;
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            uint16_t code = dynamic_keymap_get_keycode(layer, row, col);
            if (code == KC_TRNS) code = dynamic_keymap_get_keycode(base, row, col);
            if ((row != 2 || col != 4) && code == MO(FN)) {
                return true;
            }
        }
    }
    return false;
}

static bool keys_released(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        if (matrix_get_row(row)) {
            return false;
        }
    }
    return true;
}

static uint8_t macro_byte(uint16_t offset) {
    uint8_t value;
    dynamic_keymap_macro_get_buffer(offset, 1, &value);
    return value;
}

static bool playback_ready(uint8_t code) {
#    ifdef BLUETOOTH_ENABLE
    return connection_get_host() == CONNECTION_HOST_BLUETOOTH ? md770_radio_key_ready(code) : md770_power_usb_ready();
#    else
    return true;
#    endif
}

static void playback_event(uint8_t code, bool down) {
    if (IS_MODIFIER_KEYCODE(code)) {
        uint8_t modifier = MOD_BIT(code);
        if (down) {
            playback_mods |= modifier & ~get_weak_mods();
            add_weak_mods(modifier);
        } else {
            del_weak_mods(modifier & playback_mods);
            playback_mods &= ~modifier;
        }
        send_keyboard_report();
    } else {
        uint8_t bit = 1U << (code % 8);
        if (down) {
            playback_keys[code / 8] |= bit;
            register_code(code);
        } else if (playback_keys[code / 8] & bit) {
            playback_keys[code / 8] &= ~bit;
            unregister_code(code);
        }
    }
}

bool md770_playback_active(void) {
    return playback_state != PLAYBACK_IDLE;
}

void md770_playback_cancel(void) {
    playback_state   = PLAYBACK_RELEASING;
    playback_pending = false;
    playback_steps = playback_step = 0;
}

void md770_playback_cancel_physical(void) {
    if (!md770_playback_active() && !playback_pending) return;
    playback_state = PLAYBACK_IDLE;
#    ifdef BLUETOOTH_ENABLE
    md770_radio_discard_reports();
#    endif
    playback_pending = false;
    playback_steps = playback_step = 0;
    for (uint16_t code = 0; code <= UINT8_MAX; ++code) {
        if (playback_keys[code / 8] & (1U << (code % 8))) unregister_code(code);
    }
    memset(playback_keys, 0, sizeof(playback_keys));
    del_weak_mods(playback_mods);
    playback_mods = 0;
    send_keyboard_report();
#    ifdef BLUETOOTH_ENABLE
    md770_radio_sync_reports();
#    endif
}

static void playback_start(void) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();
    uint8_t  slot     = playback_pending_slot;
    playback_offset   = 0;
    while (slot && playback_offset < capacity - 1) {
        if (!macro_byte(playback_offset++)) --slot;
    }
    playback_end = playback_offset;
    while (playback_end < capacity - 1 && macro_byte(playback_end))
        ++playback_end;
    playback_state   = !slot && playback_end < capacity - 1 ? PLAYBACK_RUNNING : PLAYBACK_IDLE;
    playback_trigger = playback_pending_trigger;
    playback_wait    = 0;
    playback_pending = false;
}

static void playback_add(uint8_t code, bool down) {
    playback_sequence[playback_steps++] = (playback_event_t){.code = code, .down = down};
}

static void playback_tap(uint8_t code) {
    if (IS_MODIFIER_KEYCODE(code) && (get_weak_mods() & MOD_BIT(code))) return;
    if (!IS_MODIFIER_KEYCODE(code) && (playback_keys[code / 8] & (1U << (code % 8)))) playback_add(code, false);
    playback_add(code, true);
    playback_sequence[playback_steps - 1].delay = code == KC_CAPS_LOCK ? TAP_HOLD_CAPS_DELAY : TAP_CODE_DELAY;
    playback_add(code, false);
}

static void playback_task(void) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();
    for (uint8_t budget = 0; budget < 16; ++budget) {
        if (playback_state == PLAYBACK_RELEASING) {
            bool released = false;
            for (uint16_t code = 0; code <= UINT8_MAX; ++code) {
                if (!(playback_keys[code / 8] & (1U << (code % 8)))) continue;
                if (!playback_ready(code)) return;
                playback_event(code, false);
                released = true;
                break;
            }
            if (released) continue;
            if (playback_mods) {
                if (!playback_ready(KC_LCTL)) return;
                del_weak_mods(playback_mods);
                playback_mods = 0;
                send_keyboard_report();
            }
            if (!playback_ready(KC_LCTL)) return;
            send_keyboard_report();
            playback_state = PLAYBACK_IDLE;
        }
        if (playback_pending) playback_start();
        if (playback_state != PLAYBACK_RUNNING || timer_elapsed32(playback_started) < playback_wait) return;
        if (playback_step < playback_steps) {
            playback_event_t event = playback_sequence[playback_step];
            if (!playback_ready(event.code)) return;
            playback_event(event.code, event.down);
            ++playback_step;
            if (event.delay) {
                playback_wait    = event.delay;
                playback_started = timer_read32();
                return;
            }
            continue;
        }
        playback_step = playback_steps = 0;
        if (playback_offset >= playback_end || macro_byte(capacity - 1)) {
            md770_playback_cancel();
            continue;
        }
        uint16_t offset = playback_offset;
        uint8_t  code   = macro_byte(offset++);
        if (!code) {
            md770_playback_cancel();
            continue;
        }
        if (code != SS_QMK_PREFIX) {
            if (code >= 128) {
                md770_playback_cancel();
                continue;
            }
            uint8_t modifiers = 0;
            if ((pgm_read_byte(&ascii_to_shift_lut[code / 8]) & (1U << (code % 8)))) modifiers |= MOD_BIT(KC_LSFT);
            if ((pgm_read_byte(&ascii_to_altgr_lut[code / 8]) & (1U << (code % 8)))) modifiers |= MOD_BIT(KC_RALT);
            modifiers &= ~get_weak_mods();
            if (modifiers & MOD_BIT(KC_LSFT)) playback_add(KC_LSFT, true);
            if (modifiers & MOD_BIT(KC_RALT)) playback_add(KC_RALT, true);
            playback_tap(pgm_read_byte(&ascii_to_keycode_lut[code]));
            if (modifiers & MOD_BIT(KC_RALT)) playback_add(KC_RALT, false);
            if (modifiers & MOD_BIT(KC_LSFT)) playback_add(KC_LSFT, false);
            if ((pgm_read_byte(&ascii_to_dead_lut[code / 8]) & (1U << (code % 8)))) playback_tap(KC_SPC);
        } else if (offset + 2 <= playback_end) {
            uint8_t command = macro_byte(offset++);
            code            = macro_byte(offset++);
            if (command == SS_DELAY_CODE) {
                uint32_t delay = 0;
                while (code >= '0' && code <= '9') {
                    if (delay > (UINT32_MAX - (code - '0')) / 10) {
                        md770_playback_cancel();
                        return;
                    }
                    delay = delay * 10 + code - '0';
                    if (offset == playback_end) break;
                    code = macro_byte(offset++);
                }
                playback_wait    = delay;
                playback_started = timer_read32();
                playback_offset  = offset;
                return;
            }
            if (command == SS_DOWN_CODE || command == SS_UP_CODE) {
                if (command == SS_DOWN_CODE && !IS_MODIFIER_KEYCODE(code) && (playback_keys[code / 8] & (1U << (code % 8)))) playback_add(code, false);
                playback_add(code, command == SS_DOWN_CODE);
            } else if (command == SS_TAP_CODE) {
                playback_tap(code);
            } else {
                md770_playback_cancel();
                continue;
            }
        } else {
            md770_playback_cancel();
            continue;
        }
        playback_offset = offset;
        playback_wait   = 0;
    }
}

static void append_event(uint8_t code, bool down) {
    if (recording_size) {
        recording[recording_size - 1].delay = ((uint32_t)(delay_ticks ? delay_ticks : 80) + 1) / 2;
        if (delay_ticks) delay_ticks = 80;
    }
    recording[recording_size++] = (recorded_event_t){.code = code, .down = down};
}

static bool record_delay(keyrecord_t *record) {
    static const struct {
        keypos_t key;
        uint16_t ticks;
    } controls[] = {
        {{.row = 13, .col = 2}, 15},
        {{.row = 10, .col = 2}, 100},
        {{.row = 15, .col = 4}, 500},
    };
    for (uint8_t i = 0; i < ARRAY_SIZE(controls); ++i) {
        if (record->event.key.row != controls[i].key.row || record->event.key.col != controls[i].key.col) continue;
        uint8_t flag = 1U << i;
        if (record->event.pressed && !layer_state_is(FN)) return false;
        if (!record->event.pressed && !(delay_keys & flag)) return false;
        delay_ticks      = MIN((uint32_t)delay_ticks + controls[i].ticks, UINT16_MAX);
        matrix_row_t bit = (matrix_row_t)1U << record->event.key.col;
        if (record->event.pressed) {
            delay_keys |= flag;
            suppressed[record->event.key.row] |= bit;
        } else {
            delay_keys &= ~flag;
            suppressed[record->event.key.row] &= ~bit;
        }
        return true;
    }
    return false;
}

static uint8_t encode_event(const recorded_event_t *event, uint8_t data[12]) {
    data[0]        = SS_QMK_PREFIX;
    data[1]        = event->down ? SS_DOWN_CODE : SS_UP_CODE;
    data[2]        = event->code;
    uint8_t length = 3;
    if (event->delay) {
        data[length++] = SS_QMK_PREFIX;
        data[length++] = SS_DELAY_CODE;
        bool started   = false;
        for (uint16_t divisor = 10000; divisor; divisor /= 10) {
            uint8_t digit = event->delay / divisor % 10;
            if (digit || started || divisor == 1) {
                data[length++] = '0' + digit;
                started        = true;
            }
        }
        data[length++] = '|';
    }
    return length;
}

static uint16_t recording_bytes(void) {
    uint16_t length = 1;
    uint8_t  data[12];
    for (uint16_t i = 0; i < recording_size; ++i)
        length += encode_event(&recording[i], data);
    return length;
}

static bool macro_offsets(uint16_t offsets[DYNAMIC_KEYMAP_MACRO_COUNT + 1]) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();
    uint16_t end      = 0;
    if (capacity < DYNAMIC_KEYMAP_MACRO_COUNT + 1 || macro_byte(capacity - 1)) {
        return false;
    }
    for (uint16_t id = 0; id < DYNAMIC_KEYMAP_MACRO_COUNT; ++id) {
        offsets[id] = end;
        do {
            if (end >= capacity - 1) {
                return false;
            }
        } while (macro_byte(end++));
    }
    offsets[DYNAMIC_KEYMAP_MACRO_COUNT] = end;
    return true;
}

static bool replace_macro(uint8_t slot, bool empty) {
    uint16_t offsets[DYNAMIC_KEYMAP_MACRO_COUNT + 1];
    if (!macro_offsets(offsets)) {
        return false;
    }
    uint16_t capacity   = dynamic_keymap_macro_get_buffer_size();
    uint16_t end        = offsets[DYNAMIC_KEYMAP_MACRO_COUNT];
    uint16_t length     = empty ? 1 : recording_bytes();
    int16_t  difference = length - (offsets[slot + 1] - offsets[slot]);
    if ((int32_t)end + difference > capacity - 1) {
        return false;
    }
    uint8_t invalid = 0xff;
    dynamic_keymap_macro_set_buffer(capacity - 1, 1, &invalid);
    // Move the tail before replacing a variable-length macro in shared storage.
    for (uint16_t i = 0; i < end - offsets[slot + 1]; ++i) {
        uint16_t source = difference > 0 ? end - i - 1 : offsets[slot + 1] + i;
        uint8_t  value  = macro_byte(source);
        dynamic_keymap_macro_set_buffer(source + difference, 1, &value);
    }
    uint16_t offset = offsets[slot];
    if (!empty) {
        for (uint16_t i = 0; i < recording_size; ++i) {
            uint8_t data[12];
            uint8_t size = encode_event(&recording[i], data);
            dynamic_keymap_macro_set_buffer(offset, size, data);
            offset += size;
        }
    }
    invalid = 0;
    dynamic_keymap_macro_set_buffer(offset, 1, &invalid);
    dynamic_keymap_macro_set_buffer(capacity - 1, 1, &invalid);
    return true;
}

static bool save_recording(void) {
    if (overflow || !presses) {
        return false;
    }
    if (presses == 1) {
        dynamic_keymap_set_keycode(target_layer, destination.row, destination.col, first_code);
        return true;
    }
    bool used[DYNAMIC_KEYMAP_MACRO_COUNT] = {0};
    for (uint8_t layer = 0; layer < dynamic_keymap_get_layer_count(); ++layer) {
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint16_t code = dynamic_keymap_get_keycode(layer, row, col);
                if (layer == target_layer && row == destination.row && col == destination.col) {
                    continue;
                }
                if (IS_QK_MACRO(code) && code - QK_MACRO < DYNAMIC_KEYMAP_MACRO_COUNT) {
                    used[code - QK_MACRO] = true;
                }
            }
        }
    }
    uint16_t offsets[DYNAMIC_KEYMAP_MACRO_COUNT + 1];
    if (!macro_offsets(offsets)) {
        return false;
    }
    uint16_t previous = dynamic_keymap_get_keycode(target_layer, destination.row, destination.col);
    uint8_t  slot     = UINT8_MAX;
    if (IS_QK_MACRO(previous)) {
        uint16_t previous_slot = previous - QK_MACRO;
        if (previous_slot < DYNAMIC_KEYMAP_MACRO_COUNT && !used[previous_slot]) {
            slot = previous_slot;
        }
    }
    for (uint16_t id = 0; id < DYNAMIC_KEYMAP_MACRO_COUNT; ++id) {
        if (!used[id] && slot == UINT8_MAX) {
            slot = id;
        }
    }
    uint32_t required = recording_bytes();
    for (uint16_t id = 0; id < DYNAMIC_KEYMAP_MACRO_COUNT; ++id) {
        if (id != slot) {
            required += used[id] ? offsets[id + 1] - offsets[id] : 1;
        }
    }
    if (slot == UINT8_MAX || required >= dynamic_keymap_macro_get_buffer_size()) {
        return false;
    }
    for (uint16_t id = 0; id < DYNAMIC_KEYMAP_MACRO_COUNT; ++id) {
        if (!used[id] && id != slot && offsets[id + 1] - offsets[id] > 1) {
            if (!replace_macro(id, true)) {
                return false;
            }
        }
    }
    if (!replace_macro(slot, false)) {
        return false;
    }
    dynamic_keymap_set_keycode(target_layer, destination.row, destination.col, QK_MACRO + slot);
    return true;
}

static void save_move(void) {
    uint8_t layer = md770_profile_layer(profile, MD770_BANK_MAIN);
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            if (profile_keycode(profile, (keypos_t){.row = row, .col = col}) == first_code) {
                uint16_t original = keycode_at_keymap_location_raw(layer, row, col);
                if (row == 2 && col == 4)
                    original = KC_RGUI;
                else if (row == 6 && col == 5)
                    original = KC_APP;
                dynamic_keymap_set_keycode(layer, row, col, original);
            }
        }
    }
    dynamic_keymap_set_keycode(layer, destination.row, destination.col, first_code);
}

void md770_programming_task(void) {
    playback_task();
    md770_bank_t bank = MD770_BANK_NONE;
    if (state == IDLE && (matrix_get_row(14) & (1U << 2))) {
        bank = md770_bank_for_state(layer_state);
        if (bank == MD770_BANK_MAIN) bank = MD770_BANK_NONE;
    }
    if (bank != move_bank) {
        move_bank    = bank;
        move_started = timer_read32();
    }
    if (bank != MD770_BANK_NONE && timer_elapsed32(move_started) >= 3000) {
        profile = md770_profile_for_state(default_layer_state);
        if (bank == MD770_BANK_FN)
            state = MOVE_FN;
        else
            state = MOVE_PN;
        md770_playback_cancel();
        clear_keyboard();
    }
    if ((state == SAVE_RECORDING || state == SAVE_MOVE) && keys_released()) {
        // QMK looks up release keycodes again, so rewrite only after every release.
        if (state == SAVE_MOVE) {
            save_move();
            state = IDLE;
        } else {
            overflow = !save_recording();
            state    = TARGET;
        }
        default_layer_set(default_layer_state);
    }
}

uint8_t md770_programming_indicator(uint8_t colour) {
    if (state == MOVE_FN || state == MOVE_PN) {
        return 1;
    }
    if (state == RECORDING) {
        return timer_elapsed32(blink_started) / 250 % 2 ? 0 : 7;
    }
    if (state == TARGET || state == SAVE_RECORDING) {
        return overflow ? 1 : 7;
    }
    return colour;
}

bool md770_programming_process(uint16_t keycode, keyrecord_t *record) {
    if (!IS_KEYEVENT(record->event) || record->event.key.row >= MATRIX_ROWS || record->event.key.col >= MATRIX_COLS) {
        return true;
    }
    keypos_t     key     = record->event.key;
    matrix_row_t bit     = (matrix_row_t)1U << key.col;
    bool         trigger = KEYEQ(key, playback_trigger);
    bool         control = keycode == MO(FN) || keycode == MO(PN) || IS_MODIFIER_KEYCODE(keycode_config(keycode));
    if (playback_state == PLAYBACK_RUNNING && !IS_QK_MACRO(keycode) && (record->event.pressed || (!trigger && !control))) {
        // QMK clears weak modifiers before processing a physical press.
        if (record->event.pressed) playback_mods = 0;
        md770_playback_cancel();
    }
    if (key.row == 4 && key.col == 4 && record->event.pressed && layer_state_is(FN)) {
        if (state != SAVE_RECORDING && state != SAVE_MOVE) {
            if (state == IDLE)
                state = TARGET;
            else
                state = IDLE;
            profile  = md770_profile_for_state(default_layer_state);
            overflow = false;
            md770_playback_cancel();
            clear_keyboard();
        }
        suppressed[key.row] |= bit;
        return false;
    }
    if (state == RECORDING && keycode != MO(FN) && keycode != MO(PN) && record_delay(record)) return false;
    if (!record->event.pressed && (suppressed[key.row] & bit)) {
        suppressed[key.row] &= ~bit;
        if (state == RECORDING && recorded_down[key.row][key.col]) {
            uint8_t code                    = recorded_down[key.row][key.col];
            recorded_down[key.row][key.col] = 0;
            --held;
            if (!overflow) {
                append_event(code, false);
            }
        }
        return false;
    }
    if (keycode == MO(FN) || keycode == MO(PN)) {
        if (keycode == MO(PN) && state == RECORDING && record->event.pressed) {
            for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
                for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                    if (recorded_down[row][col] && !overflow) {
                        append_event(recorded_down[row][col], false);
                    }
                }
            }
            memset(recorded_down, 0, sizeof(recorded_down));
            state = SAVE_RECORDING;
            suppressed[key.row] |= bit;
            return false;
        }
        return true;
    }
    if (state == IDLE) {
        if (IS_QK_MACRO(keycode)) {
            if (record->event.pressed && keycode - QK_MACRO < DYNAMIC_KEYMAP_MACRO_COUNT && ((!has_anykey() && !host_last_consumer_usage() && !host_last_system_usage()) || md770_playback_active())) {
                md770_playback_cancel();
                playback_pending_slot    = keycode - QK_MACRO;
                playback_pending_trigger = key;
                playback_trigger         = key;
                playback_pending         = true;
            }
            return false;
        }
        return true;
    }
    if (!record->event.pressed) {
        return true;
    }
    suppressed[key.row] |= bit;
    if (state == SAVE_RECORDING || state == SAVE_MOVE || keycode_at_keymap_location_raw(BASE, key.row, key.col) == KC_NO) {
        return false;
    }
    if (state == TARGET || state == MOVE_FN || state == MOVE_PN) {
        uint16_t assigned = profile_keycode(profile, key);
        if (assigned == MO(FN) || assigned == MO(PN)) {
            return false;
        }
    }
    switch (state) {
        case MOVE_FN:
        case MOVE_PN:
            if ((key.row == 14 && key.col == 2) || (key.row == 5 && key.col == 3) || (key.row == 4 && key.col == 4)) {
                return false;
            }
            destination = key;
            if (state == MOVE_FN)
                first_code = MO(FN);
            else
                first_code = MO(PN);
            state = SAVE_MOVE;
            return false;
        case TARGET:
            destination    = key;
            target_layer   = md770_profile_layer(profile, md770_bank_for_state(layer_state));
            state          = RECORDING;
            recording_size = presses = held = 0;
            delay_ticks = delay_keys = 0;
            overflow                 = false;
            blink_started            = timer_read32();
            memset(recorded_down, 0, sizeof(recorded_down));
            return false;
        case RECORDING:
            break;
        default:
            return false;
    }
    keycode         = source_keycode(key);
    uint16_t output = keycode_config(keycode);
    if (keycode <= KC_TRNS || keycode > UINT8_MAX || output <= KC_TRNS || output > UINT8_MAX || recording_size + 2U + held > ARRAY_SIZE(recording)) {
        overflow = true;
        return false;
    }
    if (!presses) {
        first_code = keycode;
    }
    if (presses < UINT8_MAX) {
        ++presses;
    }
    ++held;
    recorded_down[key.row][key.col] = output;
    append_event(output, true);
    return false;
}
#endif
