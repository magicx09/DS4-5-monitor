// Per-paired-controller preferences. Board/display options stay in Config_body.
// Append-only versioned storage lives in page 2 of the existing config sector.
#pragma once
#include <cstdint>
#include "config.h"

#define CONTROLLER_PROFILE_FIELDS(X) \
    X(float, haptics_gain) X(float, haptics_gain_right) X(float, speaker_volume) \
    X(uint8_t, inactive_time) X(uint8_t, disable_inactive_disconnect) \
    X(uint8_t, polling_rate_mode) X(uint8_t, audio_buffer_length) X(uint8_t, controller_mode) \
    X(uint8_t, auto_haptics_enable) X(uint8_t, auto_haptics_gain) X(uint8_t, auto_haptics_lowpass) \
    X(uint8_t, lightbar_mode) X(uint8_t, bt_mic_enable) X(uint8_t, trigger_reduce) \
    X(uint8_t, speaker_gain) X(uint8_t, lock_volume) X(uint8_t, mic_select) \
    X(uint8_t, speaker_select) X(uint8_t, mic_level) X(uint8_t, mic_echo_cancel) \
    X(uint8_t, packet_mode) X(uint8_t, disconnect_on_dock)

struct __attribute__((packed)) ControllerProfile {
#define PROFILE_MEMBER(type, name) type name;
    CONTROLLER_PROFILE_FIELDS(PROFILE_MEMBER)
#undef PROFILE_MEMBER
    uint8_t lb_fav_r[4], lb_fav_g[4], lb_fav_b[4];
};
static_assert(sizeof(ControllerProfile) == 43);

inline ControllerProfile controller_profile(const Config_body &body) {
    ControllerProfile p{};
#define PROFILE_TAKE(type, name) p.name = body.name;
    CONTROLLER_PROFILE_FIELDS(PROFILE_TAKE)
#undef PROFILE_TAKE
    for (int i = 0; i < 4; ++i) {
        p.lb_fav_r[i] = body.lb_fav_r[i]; p.lb_fav_g[i] = body.lb_fav_g[i]; p.lb_fav_b[i] = body.lb_fav_b[i];
    }
    return p;
}

inline void apply_controller_profile(Config_body &body, const ControllerProfile &p) {
#define PROFILE_APPLY(type, name) body.name = p.name;
    CONTROLLER_PROFILE_FIELDS(PROFILE_APPLY)
#undef PROFILE_APPLY
    for (int i = 0; i < 4; ++i) {
        body.lb_fav_r[i] = p.lb_fav_r[i]; body.lb_fav_g[i] = p.lb_fav_g[i]; body.lb_fav_b[i] = p.lb_fav_b[i];
    }
}
#undef CONTROLLER_PROFILE_FIELDS
