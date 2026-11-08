#include <array>
#include <cstring>
#include "test.h"
#include "config.h"
#include "state_mgr.h"

uint8_t reportSeqCounter = 0;
uint8_t g_last_out02[47]{};
uint8_t interrupt_in_data[63]{};
bool g_lightbar_override = false;
bool g_bt_ds4 = false;
static Config_body config{};
static std::array<uint8_t, 78> sent{};
static unsigned sends = 0;
static bool connected = true, mounted = true, queue_full = false;
const Config_body &get_config() { return config; }
bool bt_is_connected() { return connected; }
bool tud_mounted() { return mounted; }
bool bt_write(const uint8_t *p, uint16_t n) {
    CHECK_EQ(n, sent.size());
    if (n != sent.size()) return false;
    std::memcpy(sent.data(), p, n);
    ++sends;
    return !queue_full;
}
static void fresh() {
    connected = false; mounted = false; queue_full = false;
    state_config_task(); // reset connection-edge state between test cases
    connected = true; mounted = true;
    config = {};
    state_init();
    state_clear_host_rumble();
    sent.fill(0);
    sends = 0;
}
static std::array<uint8_t, 47> replay() {
    std::array<uint8_t, 47> ss{};
    state_set(ss.data(), ss.size());
    state_stamp_config(ss.data(), false); // same steps as bt_send_state()
    return ss;
}

TEST(native_game_audio_survives_later_connect_state_and_led_report) {
    fresh();
    uint8_t game[47]{};
    game[0] = 0xB0; // headphone volume, speaker volume, output routing
    game[1] = 0x82; // audio mute and speaker pre-gain
    game[4] = 45; game[5] = 72; game[7] = 0x38;
    game[9] = 0x50; // mute mic/headphones, keep speaker enabled
    game[37] = 0x0B;
    state_update(game, sizeof(game));
    // Games commonly send unrelated LED/trigger updates after one-time audio init.
    uint8_t led[47]{}; led[1] = 4; led[44] = 90;
    state_update(led, sizeof(led));
    const auto ss = replay();
    CHECK_EQ(ss[4], 45); CHECK_EQ(ss[5], 72);
    CHECK_EQ(ss[7], 0x38); CHECK_EQ(ss[9], 0x50);
    CHECK_EQ(ss[37], 0x0B); CHECK_EQ(ss[44], 90);
    CHECK_EQ(sends, 0); // caching does not generate duplicate game output
}

TEST(native_audio_ignores_unflagged_and_short_host_fields) {
    fresh();
    auto before = replay();
    uint8_t game[47]; std::memset(game, 0xFF, sizeof(game));
    state_update(game, 46);
    CHECK(replay() == before);
    game[0] = 0; game[1] = 0; game[38] = 0;
    state_update(game, sizeof(game));
    const auto ss = replay();
    for (int i : {4, 5, 6, 7, 9, 37}) CHECK_EQ(ss[i], before[i]);
}

TEST(native_audio_volume_lock_applies_to_replay_as_well_as_forwarding) {
    fresh();
    config.lock_volume = 1;
    const auto before = replay();
    uint8_t game[47]{};
    game[0] = 0xF0; game[1] = 0x82;
    game[7] = 0x38; game[9] = 0xFF; game[37] = 3;
    state_update(game, sizeof(game));
    const auto ss = replay();
    for (int i : {4, 5, 6, 9}) CHECK_EQ(ss[i], before[i]);
    CHECK_EQ(ss[7], 0x38); CHECK_EQ(ss[37], 3);
    state_stamp_config(game, true);
    CHECK_EQ(game[0] & 0x70, 0); CHECK_EQ(game[1] & 2, 0);
}

TEST(native_uac_mute_preserves_game_haptic_and_power_bits) {
    fresh();
    uint8_t game[47]{}; game[1] = 2; game[9] = 0x85;
    state_update(game, sizeof(game));
    state_set_audio_mute(true, true);
    CHECK_EQ(sends, 1); CHECK_EQ(sent[0], 0x31);
    CHECK_EQ(sent[4], 2); CHECK_EQ(sent[3 + 9], 0xF5);
    state_set_audio_mute(false, false);
    CHECK_EQ(sent[3 + 9], 0x85);
    CHECK_EQ(replay()[9], 0x85);
}

TEST(native_audio_new_controller_restores_uac_not_previous_game_mute) {
    fresh();
    const auto initial = replay();
    state_set_volume(60, 50);
    state_set_mic_volume(20);
    state_set_audio_mute(true, false);
    uint8_t game[47]{}; game[0] = 0xF0; game[1] = 0x82;
    game[6] = 7; game[7] = 0x38; game[9] = 0x68; game[37] = 7;
    state_update(game, sizeof(game));
    CHECK_EQ(replay()[5], 0); CHECK_EQ(replay()[9], 0x68);
    CHECK_EQ(state_get_pc_mic_volume(), 20);
    state_clear_host_rumble();
    const auto ss = replay();
    CHECK_EQ(ss[4], 60); CHECK_EQ(ss[5], 50); CHECK_EQ(ss[6], 20);
    CHECK_EQ(ss[9], 0x10); CHECK_EQ(ss[7], initial[7]); CHECK_EQ(ss[37], initial[37]);
}

TEST(native_audio_forced_gain_keeps_game_routing_on_replay) {
    fresh();
    config.speaker_gain = 5;
    uint8_t game[47]{}; game[0] = 0x80; game[1] = 0x80;
    game[7] = 0x38; game[37] = 0x08;
    state_update(game, sizeof(game));
    CHECK_EQ(replay()[37], 0x0D); CHECK_EQ(replay()[7], 0x38);
    config.speaker_gain = 0;
    CHECK_EQ(replay()[37], 0x08);
}

TEST(volume_lock_recovers_game_zero_without_a_windows_volume_toggle) {
    fresh();
    state_set_volume(110, 86);
    state_config_task(); state_config_task(); // mounted before enabling lock
    uint8_t game[47]{}; game[0] = 0x30; // one-time game init zeros both amps
    state_update(game, sizeof(game));
    CHECK_EQ(replay()[5], 0);
    config.lock_volume = 1;
    // Even a connect-state replay before the next main loop uses USB values.
    CHECK_EQ(replay()[4], 110); CHECK_EQ(replay()[5], 86);
    state_config_task();
    CHECK_EQ(sent[3] & 0xF0, 0x30);
    CHECK_EQ(sent[3 + 4], 110); CHECK_EQ(sent[3 + 5], 86);
    CHECK_EQ(sent[3 + 1], 0); // no mute, route, mic or trigger writes
    const auto count = sends;
    for (int i = 0; i < 8; ++i) state_config_task();
    CHECK_EQ(sends, count); // no volume-spam loop
    state_update(game, sizeof(game));
    state_stamp_config(game, true);
    CHECK_EQ(game[0] & 0x30, 0); // later game initialization cannot clear amps
    CHECK_EQ(replay()[5], 86);
}

TEST(volume_lock_retries_queue_full_and_recovers_after_reconnect) {
    fresh();
    state_set_volume(90, 70);
    config.lock_volume = 1;
    state_config_task(); state_config_task(); // first sync and mute LED
    connected = false; state_config_task();
    connected = true; state_config_task();
    queue_full = true; state_config_task();
    const auto failed_count = sends;
    state_config_task(); CHECK_EQ(sends, failed_count + 1);
    queue_full = false; state_config_task();
    CHECK_EQ(sent[3] & 0xF0, 0x30); CHECK_EQ(sent[3 + 5], 70);
    const auto accepted = sends;
    state_config_task(); CHECK_EQ(sends, accepted);
    state_set_volume(65, 51); // Windows slider still works while locked
    CHECK_EQ(sent[3 + 5], 51); CHECK_EQ(replay()[5], 51);
}

TEST(volume_lock_off_preserves_game_volume_and_cancels_pending_recovery) {
    fresh();
    state_config_task(); state_config_task();
    config.lock_volume = 1; queue_full = true; state_config_task();
    config.lock_volume = 0; queue_full = false;
    const auto count = sends; state_config_task(); CHECK_EQ(sends, count);
    uint8_t game[47]{}; game[0] = 0x30; game[5] = 31;
    state_update(game, sizeof(game));
    state_stamp_config(game, true);
    CHECK_EQ(game[0] & 0x30, 0x30); CHECK_EQ(replay()[5], 31);
}

TEST(disabled_grip_is_not_reenabled_by_firmware_volume_reports) {
    fresh();
    g_last_out02[0] = 3; g_last_out02[2] = 180; g_last_out02[3] = 90;
    config.haptics_gain = 0; config.haptics_gain_right = 1;
    state_set_volume(100, 80);
    CHECK_EQ(sent[3 + 2], 180); CHECK_EQ(sent[3 + 3], 0);
    config.haptics_gain_right = 0;
    state_set_volume(100, 70);
    CHECK_EQ(sent[3 + 2], 0); CHECK_EQ(sent[3 + 3], 0);
    config.haptics_gain = 2;
    state_set_volume(100, 60);
    CHECK_EQ(sent[3 + 3], 180); // scale the original, not an already scaled copy
}
