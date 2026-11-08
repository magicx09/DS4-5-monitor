// Discovery is only a candidate selection step, not model authentication.
// Ordinary/RSSI inquiry results have no EIR name. Do not reject a DS4 just
// because its name is absent: probe its features before exposing USB/saving.
#pragma once
#include "bt_policy.h"
#include "ds4_protocol.h"

namespace ds4 {
enum class Candidate { Reject, Ds5, Ds4, ProbeDs4 };
inline Candidate discover(uint32_t cod, const uint8_t *name, uint8_t len,
                          bool enabled, bool have_id = false,
                          uint16_t source = 0, uint16_t vendor = 0, uint16_t product = 0) {
    while (name && len && name[len - 1] == 0) --len;
    if ((cod & 0x000f00) != 0x000500) return Candidate::Reject;
    if (btpolicy::inquiry_name_is_dualsense(name, len)) return Candidate::Ds5;
    if (!enabled) return Candidate::Reject;
    // EIR Device ID uses source 2 for USB VID/PID. A known different identity
    // should never be guessed as DS4, even if it has a generic controller name.
    if (have_id && source == 2) {
        return vendor == 0x054c && (product == 0x05c4 || product == 0x09cc)
            ? Candidate::Ds4 : Candidate::Reject;
    }
    if (candidate_name(name, len)) return Candidate::Ds4;
    // Unlike the donor's broad peripheral filter, this fallback is gamepad
    // class only; keyboards, mice and explicit other names remain excluded.
    if ((!name || len == 0) && (cod & 0x001ffc) == 0x000508) return Candidate::ProbeDs4;
    return Candidate::Reject;
}
} // namespace ds4
