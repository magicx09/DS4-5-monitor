//
// Created by awalol on 2026/5/15.
//

#include <cstddef>
#include <cstring>

#include "utils.h"
#include "state_mgr.h"
#include "state_override.h"
#include "rumble_scale.h"
#include "config.h"
#include "bt.h"
#include "device/usbd.h"

extern uint8_t reportSeqCounter;

// Set by the OLED lightbar service (src/oled.cpp). While true, the firmware
// owns the lightbar (an OLED mode or the charging pulse) and the host's
// AllowLedColor writes are suppressed below so they can't stomp it.
extern bool g_lightbar_override;

namespace {
    constexpr size_t kAudioControlOffset = offsetof(SetStateData, MuteLightMode) - sizeof(uint8_t);
    constexpr size_t kMuteControlOffset = offsetof(SetStateData, RightTriggerFFB) - sizeof(uint8_t);
    constexpr size_t kMotorPowerLevelOffset = offsetof(SetStateData, HostTimestamp) + sizeof(uint32_t);
    constexpr size_t kAudioControl2Offset = kMotorPowerLevelOffset + sizeof(uint8_t);
    constexpr size_t kHapticLowPassFilterOffset = offsetof(SetStateData, LightFadeAnimation) - 2 * sizeof(uint8_t);
    constexpr size_t kPlayerIndicatorsOffset = offsetof(SetStateData, LedRed) - sizeof(uint8_t);
}

// Headphones/Speaker volume start at max until the host sets its USB volume,
// which state_set_volume() then maps onto the controller's amp (see usb.cpp).
// With the allow bits clear the DS5 amp stays silent, so they must stay set.
static constexpr uint8_t state_init_data[63] = {
    0xfd, 0xf7, 0x0, 0x0,
    0x7f, 0x64, // Headphones, Speaker
    0x40, 0x9, 0x0, 0x00, 0x0, 0x0, 0x0, 0x0, // VolumeMic=64, MuteControl all clear (no PowerSave)
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa,
    0x7, 0x0, 0x0, 0x2, 0x1,
    0x00,
    0xff, 0xd7, 0x00 // RGB LED: R, G, B (Nijika Color!)✨
};

uint8_t state[63]{};

// USB endpoint controls survive a controller reconnect; per-game HID audio
// controls belong to the current controller session only.
static uint8_t s_usb_headphones = 0x7f, s_usb_speaker = 0x64, s_usb_mic = 0x40;
static uint8_t s_usb_mute = 0;
static HostAudioBytes s_host_audio = kHostAudioDefaults;

void state_init() {
    memcpy(state, state_init_data, sizeof(state));
    s_usb_headphones = 0x7f;
    s_usb_speaker = 0x64;
    s_usb_mic = 0x40;
    s_usb_mute = 0;
    s_host_audio = kHostAudioDefaults;
}

void state_set(uint8_t *data, const uint8_t size) {
    if (size > 63) {
        printf("[StateMgr] Warning: State Set over 63 bytes\n");
    }
    memcpy(data, state, size);
}

void state_set_led(uint8_t r, uint8_t g, uint8_t b) {
    state[offsetof(SetStateData, LedRed) + 0] = r;
    state[offsetof(SetStateData, LedRed) + 1] = g;
    state[offsetof(SetStateData, LedRed) + 2] = b;
}

// One 0x31 with just these SetStateData bytes (their allow bits say what the
// controller applies), carrying the game's current rumble.
static bool send_state_report(const uint8_t *ss) {
    uint8_t pkt[78]{};
    pkt[0] = 0x31;
    pkt[1] = reportSeqCounter << 4;
    reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
    pkt[2] = 0x10;
    memcpy(pkt + 3, ss, sizeof(SetStateData));
    state_stamp_host_rumble(pkt + 3);
    // error: LOGGED-OK — counter bt_send_drops (0xFD, OLED Diagnostics); a full output queue drops it, the next state change sends the whole state again
    return bt_write(pkt, sizeof(pkt));
}

void state_set_volume(uint8_t headphones, uint8_t speaker) {
    s_usb_headphones = headphones;
    s_usb_speaker = speaker;
    state[0] |= 0x30; // AllowHeadphoneVolume | AllowSpeakerVolume
    state[offsetof(SetStateData, VolumeHeadphones)] = headphones;
    state[offsetof(SetStateData, VolumeSpeaker)] = speaker;

    // Push it now (upstream update_state): audio frames don't carry state.
    uint8_t ss[sizeof(SetStateData)]{};
    ss[0] = 0x30; // valid_flag0: headphone + speaker volume only
    ss[offsetof(SetStateData, VolumeHeadphones)] = headphones;
    ss[offsetof(SetStateData, VolumeSpeaker)] = speaker;
    send_state_report(ss);
}

void state_set_audio_mute(bool mic, bool speaker) {
    // MuteControl [9]: bit4 MicMute, bit5 SpeakerMute, bit6 HeadphoneMute.
    s_usb_mute = (uint8_t) ((mic ? 0x10 : 0) | (speaker ? 0x60 : 0));
    // A UAC mute changes only the audio mute bits, not the game's haptic
    // mute or sensor/audio power-save bits in this same byte.
    const uint8_t mute = (state[kMuteControlOffset] & ~0x70) | s_usb_mute;
    state[kMuteControlOffset] = mute;  // the connect-time state carries it too
    uint8_t ss[sizeof(SetStateData)]{};
    ss[1] = 0x02;  // AllowAudioMute
    ss[kMuteControlOffset] = mute;
    send_state_report(ss);
}

void state_set_mic_volume(uint8_t volume) {
    s_usb_mic = volume;
    // Kept as the PC's level (the connect-time state carries it, stamped with
    // a fixed mic level when one is set), but with a fixed level (1.16) the
    // controller keeps that one; Auto puts this back (state_config_task).
    state[offsetof(SetStateData, VolumeMic)] = volume;
    if (get_config().mic_level) return;
    uint8_t ss[sizeof(SetStateData)]{};
    ss[0] = 0x40;  // AllowMicVolume
    ss[offsetof(SetStateData, VolumeMic)] = volume;
    send_state_report(ss);
}

static StateOverrides overrides_from_config() {
    const Config_body &c = get_config();
    return {c.trigger_reduce, c.speaker_gain, c.lock_volume, c.mic_select, c.mic_level, c.mic_echo_cancel};
}

void state_stamp_config(uint8_t *ss, bool from_host) {
    // With volume lock enabled the USB endpoint owns the playback amps.
    // A full-state replay must not resurrect the game's pre-lock zero volume.
    // Do not add allow bits to unrelated reports or modify the audio routing.
    if (!from_host && get_config().lock_volume) {
        if (ss[0] & state_bits::kF0AllowHeadphoneVolume)
            ss[offsetof(SetStateData, VolumeHeadphones)] = s_usb_headphones;
        if (ss[0] & state_bits::kF0AllowSpeakerVolume)
            ss[offsetof(SetStateData, VolumeSpeaker)] = s_usb_speaker;
    }
    stamp_overrides(ss, overrides_from_config(), s_host_audio, from_host);
}

extern uint8_t interrupt_in_data[63]; // main.cpp: the controller's latest input report

void state_config_task() {
    if (bt_is_ds4()) return;
    static bool s_was_connected = false;
    static StateOverrides s_sent{};
    static bool s_volume_sync_pending = false;
    static int8_t s_muted = -1;  // the controller's mic mute as last shown on its light; -1 unknown
    // After USB is up: by then the connect-time feature reads are done (BT
    // traffic during them delays controller detection, see audio_loop).
    const bool connected = bt_is_connected() && tud_mounted();
    const StateOverrides now = overrides_from_config();
    if (!connected || !s_was_connected) {
        // The connect-time state (bt_send_state) already carries the settings.
        s_was_connected = connected;
        s_sent = now;
        s_muted = -1;
        // Reapply after every new mounted connection as well as when the
        // option is enabled live. No mic control or new audio stream is opened.
        s_volume_sync_pending = connected && now.lock_volume;
        return;
    }
    const bool trigger = now.trigger_reduce != s_sent.trigger_reduce;
    const bool gain = now.speaker_gain != s_sent.speaker_gain;
    // The mic choice and the echo cancellation (1.17) share AudioControl.
    const bool mic = now.mic_select != s_sent.mic_select || now.echo_cancel != s_sent.echo_cancel;
    const bool level = now.mic_level != s_sent.mic_level;
    if (now.lock_volume && !s_sent.lock_volume) s_volume_sync_pending = true;
    if (!now.lock_volume) s_volume_sync_pending = false;
    s_sent = now;
    if (s_volume_sync_pending) {
        uint8_t ss[sizeof(SetStateData)]{};
        ss[0] = state_bits::kF0AllowHeadphoneVolume | state_bits::kF0AllowSpeakerVolume;
        state_stamp_config(ss, false);
        // A full Bluetooth queue must not lose the one recovery write.
        // Retry next loop; once accepted, no periodic volume forcing is needed.
        if (send_state_report(ss)) {
            state[offsetof(SetStateData, VolumeHeadphones)] = s_usb_headphones;
            state[offsetof(SetStateData, VolumeSpeaker)] = s_usb_speaker;
            s_volume_sync_pending = false;
        }
    }
    if (trigger || gain || mic || level) {
        uint8_t ss[sizeof(SetStateData)]{};
        build_override_push(ss, now, s_host_audio, trigger, gain, mic, level, s_usb_mic);
        send_state_report(ss);
    }
    // The mute button's light follows the controller's mic mute (input report
    // [53] bit 2), as on a wired DualSense (upstream a7824d9).
    const int8_t muted = (interrupt_in_data[53] >> 2) & 1;
    if (muted != s_muted) {
        s_muted = muted;
        uint8_t ss[sizeof(SetStateData)]{};
        ss[1] = 0x01;  // AllowMuteLight
        ss[offsetof(SetStateData, MuteLightMode)] = muted ? MuteLight::On : MuteLight::Off;
        send_state_report(ss);
    }
}

// The host's most recent 0x02 output report (SetStateData), kept by main.cpp.
extern uint8_t g_last_out02[47];

void state_stamp_host_rumble(uint8_t *ss) {
    const uint8_t *host = g_last_out02;
    // flags0 bit 0 EnableRumbleEmulation, bit 1 UseRumbleNotHaptics
    ss[0] = (uint8_t) ((ss[0] & ~0x03) | (host[0] & 0x03));
    ss[offsetof(SetStateData, RumbleEmulationRight)] = scale_rumble(host[offsetof(SetStateData, RumbleEmulationRight)], get_config().haptics_gain_right);
    ss[offsetof(SetStateData, RumbleEmulationLeft)] = scale_rumble(host[offsetof(SetStateData, RumbleEmulationLeft)], get_config().haptics_gain);
    // byte 38 bit 2 EnableImprovedRumbleEmulation, bit 3 UseRumbleNotHaptics2
    ss[38] = (uint8_t) ((ss[38] & ~0x0C) | (host[38] & 0x0C));
}

void state_clear_host_rumble() {
    memset(g_last_out02, 0, sizeof(g_last_out02));
    s_host_audio = kHostAudioDefaults;
    state[offsetof(SetStateData, VolumeHeadphones)] = s_usb_headphones;
    state[offsetof(SetStateData, VolumeSpeaker)] = s_usb_speaker;
    state[offsetof(SetStateData, VolumeMic)] = s_usb_mic;
    state[kAudioControlOffset] = state_init_data[kAudioControlOffset];
    state[kAudioControl2Offset] = state_init_data[kAudioControl2Offset];
    state[kMuteControlOffset] = s_usb_mute;
}

void state_get_led(uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = state[offsetof(SetStateData, LedRed) + 0];
    *g = state[offsetof(SetStateData, LedRed) + 1];
    *b = state[offsetof(SetStateData, LedRed) + 2];
}

uint8_t state_get_pc_mic_volume() {
    return s_usb_mic;
}

uint8_t state_get_player_leds() {
    return state[offsetof(SetStateData, LedRed) - 1] & 0x1F;
}

void state_update(const uint8_t *data, const uint8_t size) {
    if (size < sizeof(SetStateData)) {
        printf(
            "[StateMgr] Error: SetStateData at least %u bytes\n",
            static_cast<unsigned>(sizeof(SetStateData))
        );
        return;
    }

    SetStateData update{};
    memcpy(&update, data, sizeof(update));
    record_host_audio(s_host_audio, data);

    const auto copy_if_allowed = [&](const bool allowed, const size_t offset, const size_t length) {
        if (allowed) {
            memcpy(state + offset, data + offset, length);
        }
    };
    auto set_bit = [](uint8_t &byte, const int bit, const bool value) {
        byte = (byte & ~(1 << bit)) | (value << bit);
    };

    set_bit(state[0], 0, update.EnableRumbleEmulation);
    set_bit(state[0], 1, update.UseRumbleNotHaptics);
    set_bit(state[38], 2, update.EnableImprovedRumbleEmulation);
    copy_if_allowed(
        update.UseRumbleNotHaptics || update.EnableRumbleEmulation,
        offsetof(SetStateData, RumbleEmulationRight),
        2
    );

    // The game's report is forwarded immediately by main.cpp, but the full
    // state is also sent after USB enumeration. Retain the allowed audio
    // fields so that replay cannot undo a game's native audio initialization.
    // Respect the same volume lock as stamp_overrides on the forwarded copy.
    const bool unlocked = !get_config().lock_volume;
    copy_if_allowed(
        update.AllowHeadphoneVolume && unlocked,
        offsetof(SetStateData, VolumeHeadphones),
        sizeof(update.VolumeHeadphones)
    );
    copy_if_allowed(
        update.AllowSpeakerVolume && unlocked,
        offsetof(SetStateData, VolumeSpeaker),
        sizeof(update.VolumeSpeaker)
    );
    copy_if_allowed(
        update.AllowMicVolume && unlocked,
        offsetof(SetStateData, VolumeMic),
        sizeof(update.VolumeMic)
    );
    copy_if_allowed(
        update.AllowAudioControl,
        kAudioControlOffset,
        sizeof(uint8_t)
    );

    copy_if_allowed(
        update.AllowMuteLight,
        offsetof(SetStateData, MuteLightMode),
        sizeof(update.MuteLightMode)
    );

    copy_if_allowed(
        update.AllowAudioMute && unlocked,
        kMuteControlOffset,
        sizeof(uint8_t)
    );

    copy_if_allowed(
        update.AllowRightTriggerFFB,
        offsetof(SetStateData, RightTriggerFFB),
        sizeof(update.RightTriggerFFB)
    );
    copy_if_allowed(
        update.AllowLeftTriggerFFB,
        offsetof(SetStateData, LeftTriggerFFB),
        sizeof(update.LeftTriggerFFB)
    );

    /*copy_if_allowed(
        update.AllowMotorPowerLevel,
        kMotorPowerLevelOffset,
        sizeof(uint8_t)
    );*/
    copy_if_allowed(
        update.AllowAudioControl2,
        kAudioControl2Offset,
        sizeof(uint8_t)
    );
    /*copy_if_allowed(
        update.AllowHapticLowPassFilter,
        kHapticLowPassFilterOffset,
        sizeof(uint8_t)
    );*/

    copy_if_allowed(
        update.AllowColorLightFadeAnimation,
        offsetof(SetStateData, LightFadeAnimation),
        sizeof(update.LightFadeAnimation)
    );
    copy_if_allowed(
        update.AllowLightBrightnessChange,
        offsetof(SetStateData, LightBrightness),
        sizeof(update.LightBrightness)
    );
    copy_if_allowed(
        update.AllowPlayerIndicators,
        kPlayerIndicatorsOffset,
        sizeof(uint8_t)
    );
    copy_if_allowed(
        update.AllowLedColor && !g_lightbar_override,
        offsetof(SetStateData, LedRed),
        sizeof(update.LedRed) * 3
    );
}
