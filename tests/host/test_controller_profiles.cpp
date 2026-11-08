#include <cstring>
#include "test.h"
#include "fakes.h"
#include "config.h"
#include "controller_profile.h"
#include "slots.h"
#include "flash_layout.h"
#include "rumble_scale.h"
#include "haptics_decimate.h"
#include "settings_menu.h"

namespace {
constexpr uint8_t ds4[6] = {1,2,3,4,5,6}, ds5[6] = {6,5,4,3,2,1}, other[6] = {9,8,7,6,5,4};
// Isolate the optional profile layer from legacy config-only fixture tests.
struct Session {
    Session() {
        g_fake_flash_fail = false;
        fake_flash_erase_all(); config_load(); slots_load();
        CHECK(slot_assign(0, ds4, true)); CHECK(slot_assign(1, ds5));
        auto c = get_config(); c.haptics_gain = c.haptics_gain_right = 2;
        c.lock_volume = 1; c.speaker_volume = -5;
        set_config(c); CHECK(config_save()); CHECK(config_profiles_init());
    }
    ~Session() { g_fake_flash_fail = false; config_load(); }
};
void reboot() { config_load(); slots_load(); CHECK(config_profiles_init()); }
void gains(float value) {
    auto c = get_config(); c.haptics_gain = c.haptics_gain_right = value; set_config(c);
}
}

TEST(profiles_upgrade_preserves_existing_preferences_in_every_slot) {
    Session session;
    for (int i = 0; i < 4; ++i) {
        CHECK(config_save_current_slot(i));
        CHECK_EQ(get_config().haptics_gain, 2);
        CHECK_EQ(get_config().lock_volume, 1);
        CHECK_EQ(get_config().speaker_volume, -5);
        CHECK(!config_unsaved());
    }
    CHECK(slot_is_ds4(0)); CHECK(!slot_is_ds4(1));
    CHECK_EQ(slot_owner_of(ds4), 0); CHECK_EQ(slot_owner_of(ds5), 1);
}

TEST(profiles_ds4_off_ds5_on_survive_switch_and_reboot) {
    Session session;
    gains(0); auto c = get_config(); c.speaker_volume = -15; c.lock_volume = 0;
    set_config(c); CHECK(config_save());
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 2);
    CHECK_EQ(get_config().lock_volume, 1);
    gains(3.5f); CHECK(config_save());
    reboot(); CHECK_EQ(get_config().current_slot, 1); CHECK_EQ(get_config().haptics_gain, 3.5f);
    CHECK(config_save_current_slot(0)); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK_EQ(get_config().haptics_gain_right, 0); CHECK_EQ(get_config().speaker_volume, -15);
    reboot(); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain_right, 3.5f);
}

TEST(profiles_unsaved_drafts_stay_in_their_slot_and_are_not_saved_by_switch) {
    Session session;
    gains(0); CHECK(config_unsaved());
    CHECK(config_save_current_slot(1)); CHECK(!config_unsaved());
    gains(4); CHECK(config_save()); // saves slot 1 only
    CHECK(config_save_current_slot(0)); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK(config_unsaved());
    reboot(); CHECK_EQ(get_config().haptics_gain, 2); CHECK(!config_unsaved());
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 4);
}

TEST(profiles_board_preferences_are_shared_and_single_field_saves_preserve_profiles) {
    Session session;
    gains(0); CHECK(config_save());
    CHECK(config_save_current_slot(1));
    auto c = get_config(); c.haptics_gain = 4; c.screen_dim_timeout = 7;
    c.screen_brightness = 2; c.disable_pico_led = 1; set_config(c); CHECK(config_save());
    CHECK(config_save_current_slot(0)); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK_EQ(get_config().screen_dim_timeout, 7); CHECK_EQ(get_config().screen_brightness, 2);
    gains(5); // unsaved, must not leak through narrator/flip/power/display saves
    CHECK(config_save_narrator(3)); CHECK(config_save_screen_flipped(true));
    CHECK(config_save_power_flags(POWER_PS_WAKES_PC)); CHECK(config_save_display_id(config_display_id()));
    reboot(); CHECK_EQ(get_config().haptics_gain, 0); CHECK(config_screen_flipped());
    CHECK_EQ(config_power_flags(), POWER_PS_WAKES_PC); CHECK_EQ(get_config().narrator_voice, 3);
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 4);
    CHECK_EQ(get_config().screen_brightness, 2);
}

TEST(profiles_all_controller_fields_follow_slot_not_the_board) {
    Session session;
    auto c = get_config(); c.trigger_reduce = 7; c.speaker_select = 1;
    c.polling_rate_mode = 3; c.lightbar_mode = 4; c.lb_fav_r[2] = 17;
    c.auto_haptics_enable = 2; c.bt_mic_enable = 0; c.disconnect_on_dock = 1;
    set_config(c); const auto expected = controller_profile(get_config()); CHECK(config_save());
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().trigger_reduce, 0);
    CHECK_EQ(get_config().lightbar_mode, 8);
    CHECK(config_save_current_slot(0)); const auto actual = controller_profile(get_config());
    CHECK(std::memcmp(&expected, &actual, sizeof(actual)) == 0);
}

TEST(profiles_forget_and_same_mac_repair_do_not_resurrect_old_settings) {
    Session session;
    gains(0); CHECK(config_save()); const auto generation = slot_generation(0);
    CHECK(slot_forget(0)); CHECK(slot_generation(0) != generation);
    CHECK_EQ(get_config().haptics_gain, 1); // a cleared slot starts fresh
    CHECK(slot_assign(0, ds4, true));
    reboot(); CHECK_EQ(get_config().haptics_gain, 1);
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 2);
}

TEST(profiles_replacing_a_controller_resets_only_its_slot) {
    Session session;
    gains(0); CHECK(config_save());
    CHECK(slot_assign(0, other)); CHECK_EQ(get_config().haptics_gain, 1);
    CHECK_EQ(slot_owner_of(other), 0); CHECK_EQ(slot_owner_of(ds4), -1);
    CHECK(config_save()); reboot(); CHECK_EQ(get_config().haptics_gain, 1);
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 2);
}

TEST(profiles_repeated_assignment_keeps_owner_preferences_and_generation) {
    Session session;
    gains(0); CHECK(config_save()); const auto generation = slot_generation(0);
    CHECK(slot_assign(0, ds4, true)); CHECK_EQ(slot_generation(0), generation);
    CHECK_EQ(get_config().haptics_gain, 0);
    reboot(); CHECK_EQ(get_config().haptics_gain, 0);
}

TEST(profiles_flash_failure_keeps_draft_dirty_and_retry_saves_it) {
    Session session;
    gains(0); g_fake_flash_fail = true; CHECK(!config_save()); CHECK(config_unsaved());
    g_fake_flash_fail = false; reboot(); CHECK_EQ(get_config().haptics_gain, 2);
    gains(0); CHECK(config_save()); CHECK(!config_unsaved());
    reboot(); CHECK_EQ(get_config().haptics_gain, 0);
}

TEST(profiles_corrupt_record_falls_back_without_harming_other_controller) {
    Session session;
    gains(0); CHECK(config_save());
    CHECK(config_save_current_slot(1)); gains(4); CHECK(config_save());
    // Record 0 begins after the 8-byte page header; flip its bound MAC.
    g_fake_flash[CONFIG_FLASH_OFFSET + FLASH_PAGE_SIZE + 8 + 5] ^= 1;
    reboot(); CHECK_EQ(get_config().haptics_gain, 4);
    CHECK(config_save_current_slot(0)); CHECK_EQ(get_config().haptics_gain, 1);
    CHECK(config_unsaved()); CHECK(config_save()); CHECK(!config_unsaved());
}

TEST(profiles_reset_current_keeps_the_other_controller_and_screen_orientation) {
    Session session;
    gains(0); CHECK(config_save()); CHECK(config_save_screen_flipped(true));
    CHECK(config_save_current_slot(1)); gains(5); CHECK(config_save());
    config_default_keep_slot(1); CHECK(config_save()); CHECK_EQ(get_config().haptics_gain, 1);
    CHECK(config_screen_flipped());
    CHECK(config_save_current_slot(0)); CHECK_EQ(get_config().haptics_gain, 0);
}

TEST(profiles_menu_zero_disables_classic_rumble_and_native_haptics) {
    Session session;
    auto c = get_config(); c.haptics_gain = 0.5f; c.haptics_gain_right = 0.5f;
    uint8_t power = 0;
    settings_menu::adjust(settings_menu::kItems[0], c, power, -1);
    settings_menu::adjust(settings_menu::kItems[1], c, power, -1);
    set_config(c); CHECK_EQ(get_config().haptics_gain, 0); CHECK_EQ(get_config().haptics_gain_right, 0);
    char label[16]; settings_menu::format_value(settings_menu::kItems[0], c, power, label, sizeof(label));
    CHECK(std::strcmp(label, "off") == 0);
    CHECK_EQ(scale_rumble(255, get_config().haptics_gain), 0);
    CHECK_EQ(haptic_to_s8(1, get_config().haptics_gain), 0);
    CHECK_EQ(haptic_to_s8(-1, get_config().haptics_gain_right), 0);
    CHECK(config_save()); CHECK(config_save_current_slot(1));
    CHECK(scale_rumble(50, get_config().haptics_gain) > 0);
}

TEST(profiles_switch_invalidates_oled_snapshot_and_pairing_prepared_empty_slot_keeps_draft) {
    Session session;
    const auto before = config_profile_generation();
    CHECK(config_save_current_slot(2)); CHECK(config_profile_generation() != before);
    gains(0); CHECK(slot_assign(2, other)); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK(config_save()); reboot(); CHECK_EQ(get_config().haptics_gain, 0);
    CHECK(config_save_current_slot(1)); CHECK_EQ(get_config().haptics_gain, 2);
}
