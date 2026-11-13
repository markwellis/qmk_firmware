// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
extern "C" {
#include "quantum.h"
#include "keyboards/mistel/md770rgb_bt/md770rgb_bt.h"
#include "keycode_config.h"
#include "keyboards/mistel/md770rgb_bt/keymaps/default/keymap.h"

void md770_set_layer_led(uint8_t colour) {
    (void)colour;
}

// The native test matrix has no separate raw snapshot.
bool peek_matrix(uint8_t row, uint8_t col, bool) {
    return (matrix_get_row(row) & (1U << col)) != 0;
}
}

class MD770DipSwitch : public testing::Test {
   protected:
    void SetUp() override {
        eeconfig_init_quantum();
        dip_switch_update_mask_user(0);
        keymap_config.raw = 0;
        layer_state_set(0);
    }
};

TEST_F(MD770DipSwitch, CapsControlSwapAndFnPrecedence) {
    dip_switch_update_mask_user(2 | 8);
    EXPECT_EQ(keycode_config(KC_CAPS), KC_LCTL);
    EXPECT_EQ(keycode_config(KC_LCTL), KC_CAPS);
    EXPECT_FALSE(layer_state_is(CAPS_FN));
    dip_switch_update_mask_user(8);
    EXPECT_EQ(keycode_config(KC_CAPS), KC_CAPS);
    EXPECT_TRUE(layer_state_is(CAPS_FN));
}

TEST_F(MD770DipSwitch, AllOffRestoresTheStandardModifiers) {
    dip_switch_update_mask_user(15);
    dip_switch_update_mask_user(0);
    EXPECT_EQ(keycode_config(KC_CAPS), KC_CAPS);
    EXPECT_EQ(keycode_config(KC_LCTL), KC_LCTL);
    EXPECT_EQ(keycode_config(KC_LALT), KC_LALT);
    EXPECT_EQ(keycode_config(KC_LGUI), KC_LGUI);
    EXPECT_EQ(keycode_config(KC_RALT), KC_RALT);
    EXPECT_EQ(keycode_config(KC_RGUI), KC_RGUI);
    EXPECT_FALSE(layer_state_is(MAC));
    EXPECT_FALSE(layer_state_is(CAPS_FN));
}

TEST_F(MD770DipSwitch, MacModeOwnsBothModifierPairsAndDoesNotDoubleSwap) {
    dip_switch_update_mask_user(1 | 4);
    EXPECT_EQ(keycode_config(KC_LALT), KC_LGUI);
    EXPECT_EQ(keycode_config(KC_LGUI), KC_LALT);
    EXPECT_EQ(keycode_config(KC_RALT), KC_RGUI);
    EXPECT_EQ(keycode_config(KC_RGUI), KC_RALT);
    EXPECT_TRUE(layer_state_is(MAC));
    dip_switch_update_mask_user(4);
    EXPECT_EQ(keycode_config(KC_LALT), KC_LGUI);
    EXPECT_EQ(keycode_config(KC_RALT), KC_RALT);
    EXPECT_FALSE(layer_state_is(MAC));
}

TEST_F(MD770DipSwitch, ChangingSwitchesClearsHeldModifiers) {
    add_mods(MOD_BIT(KC_LALT) | MOD_BIT(KC_LCTL));
    ASSERT_NE(get_mods(), 0);
    dip_switch_update_mask_user(4);
    EXPECT_EQ(get_mods(), 0);
    EXPECT_EQ(keycode_config(KC_LALT), KC_LGUI);
}

TEST_F(MD770DipSwitch, PreservesOtherConfigurationAndActiveLayers) {
    keymap_config.nkro                 = true;
    keymap_config.no_gui               = true;
    keymap_config.swap_escape_capslock = true;
    layer_state_set((1UL << FN) | (1UL << PN));
    dip_switch_update_mask_user(1 | 8);
    EXPECT_TRUE(keymap_config.nkro);
    EXPECT_TRUE(keymap_config.no_gui);
    EXPECT_TRUE(keymap_config.swap_escape_capslock);
    EXPECT_TRUE(layer_state_is(FN));
    EXPECT_TRUE(layer_state_is(PN));
    EXPECT_TRUE(layer_state_is(MAC));
    EXPECT_TRUE(layer_state_is(CAPS_FN));
}

TEST_F(MD770DipSwitch, MagicGuiLockKeepsDipSwapsWithoutPersistingThem) {
    dip_switch_update_mask_user(1 | 2);
    keyrecord_t record   = {};
    record.event.pressed = true;
    EXPECT_FALSE(process_record_user(GU_TOGG, &record));
    EXPECT_TRUE(keymap_config.no_gui);
    EXPECT_TRUE(keymap_config.swap_control_capslock);
    EXPECT_TRUE(keymap_config.swap_lalt_lgui);
    EXPECT_TRUE(keymap_config.swap_ralt_rgui);
    keymap_config_t stored = {};
    eeconfig_read_keymap(&stored);
    EXPECT_TRUE(stored.no_gui);
    EXPECT_FALSE(stored.swap_control_capslock);
    EXPECT_FALSE(stored.swap_lalt_lgui);
    EXPECT_FALSE(stored.swap_ralt_rgui);
}

TEST_F(MD770DipSwitch, LayoutCycleUsesPersistentQmkDefaultLayers) {
    default_layer_set(1UL << BASE);
    keyrecord_t record   = {};
    record.event.pressed = true;
    for (uint8_t layer : {COLEMAK, DVORAK, BASE}) {
        EXPECT_FALSE(process_record_user(MD770_NEXT_LAYOUT, &record));
        EXPECT_EQ(default_layer_state, 1UL << layer);
        EXPECT_EQ(eeconfig_read_default_layer(), 1UL << layer);
    }
    record.event.pressed = false;
    EXPECT_FALSE(process_record_user(MD770_NEXT_LAYOUT, &record));
    EXPECT_EQ(default_layer_state, 1UL << BASE);
}

TEST_F(MD770DipSwitch, LayoutCyclingPreservesTheSelectedProfile) {
    default_layer_set((1UL << BASE) | (1UL << PROFILE2));
    keyrecord_t record   = {};
    record.event.pressed = true;
    EXPECT_FALSE(process_record_user(MD770_NEXT_LAYOUT, &record));
    EXPECT_EQ(default_layer_state, (1UL << COLEMAK) | (1UL << PROFILE2));
    EXPECT_EQ(eeconfig_read_default_layer(), default_layer_state);
    process_record_user(MD770_NEXT_LAYOUT, &record);
    EXPECT_EQ(default_layer_state, (1UL << DVORAK) | (1UL << PROFILE2));
    process_record_user(MD770_NEXT_LAYOUT, &record);
    EXPECT_EQ(default_layer_state, (1UL << BASE) | (1UL << PROFILE2));
}

TEST_F(MD770DipSwitch, ProfileSelectionPreservesTheLayoutAndTransientLayers) {
    default_layer_set(1UL << DVORAK);
    layer_state_set((1UL << FN) | (1UL << PN));
    keyrecord_t record   = {};
    record.event.pressed = true;
    for (uint16_t code = MD770_PROFILE1; code <= MD770_PROFILE3; ++code) {
        EXPECT_FALSE(process_record_user(code, &record));
        EXPECT_EQ(default_layer_state, (1UL << DVORAK) | (1UL << (PROFILE1 + code - MD770_PROFILE1)));
        EXPECT_EQ(eeconfig_read_default_layer(), default_layer_state);
        EXPECT_TRUE(layer_state_is(FN));
        EXPECT_TRUE(layer_state_is(PN));
    }
    EXPECT_FALSE(process_record_user(MD770_PROFILE0, &record));
    EXPECT_EQ(default_layer_state, 1UL << DVORAK);
}

TEST_F(MD770DipSwitch, ProfileReleaseDoesNotChangeTheSelection) {
    default_layer_set((1UL << BASE) | (1UL << PROFILE1));
    keyrecord_t record = {};
    EXPECT_FALSE(process_record_user(MD770_PROFILE3, &record));
    EXPECT_EQ(default_layer_state, (1UL << BASE) | (1UL << PROFILE1));
}
