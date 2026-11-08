// DS4 wire layouts adapted from snipem/DS4Dongle, commit 2b1e3078.
// See DS4Dongle-LICENSE.txt. Pure helpers: no SDK, heap, or flash writes.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ds4 {
constexpr size_t kAudioReportBytes = 462; // fixed report size: includes 4 padding bytes before CRC
constexpr size_t kSbcFrameBytes = 112;
constexpr size_t kAudioFrames = 4;
inline bool valid_audio_output(const uint8_t *p, size_t n) {
    return p && n == kAudioReportBytes && p[0] == 0x17;
}
inline uint8_t audio_target(uint8_t selection, bool headset) {
    if (selection == 3) return 0;
    return selection == 2 || (selection == 0 && headset) ? 0x24 : 0x02;
}
// DS4Dongle's measured headphone mapping: advertised -73..-1 dB maps
// to hardware bytes 20..80. The built-in speaker uses the same control.
inline uint8_t audio_level(float db, bool muted) {
    if (muted) return 0;
    if (!(db >= -73.0f)) db = -73.0f;
    if (db > -1.0f) db = -1.0f;
    return static_cast<uint8_t>(20.0f + (db + 73.0f) * (60.0f / 72.0f) + 0.5f);
}
// Firmware-owned volume command: separate from the restricted host effects
// path below. Never enables microphone updates, rumble or lightbar changes.
inline void audio_volume(uint8_t target, uint8_t level, uint8_t *out) {
    std::memset(out, 0, 78);
    out[0] = 0x11; out[1] = 0xc0; out[2] = 0x20;
    out[3] = 0xb0;
    if (level > 80) level = 80;
    if (target == 0x24) out[21] = out[22] = level;
    if (target == 0x02) out[24] = level;
}
inline void audio_header(uint8_t *out, uint16_t counter, uint8_t target) {
    std::memset(out, 0, kAudioReportBytes);
    out[0] = 0x17; out[1] = 0x40; out[2] = 0xa0;
    out[3] = static_cast<uint8_t>(counter); out[4] = static_cast<uint8_t>(counter >> 8);
    out[5] = target;
}
static_assert(6 + kAudioFrames * kSbcFrameBytes + 4 + 4 == kAudioReportBytes);
constexpr size_t kUsbPayload = 63;
constexpr size_t kBtInput = 79; // A1 + report 0x11 (78 bytes, including CRC)
inline bool candidate_name(const uint8_t *s, size_t n) {
    constexpr char name[] = "Wireless Controller";
    return s && n == sizeof(name) - 1 && std::memcmp(s, name, n) == 0;
}
inline uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int b = 0; b < 8; ++b) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}
inline bool valid_crc(const uint8_t *p, size_t n) {
    if (!p || n < 5) return false;
    const uint8_t *t = p + n - 4;
    const uint32_t want = uint32_t(t[0]) | uint32_t(t[1]) << 8 | uint32_t(t[2]) << 16 | uint32_t(t[3]) << 24;
    return crc(p, n - 4) == want;
}
// Linux hid-playstation: dualshock4_get_firmware_info requests 49 bytes
// (report ID included) with CRC disabled. Calibration BT 0x05 has 41 bytes
// and CRC enabled. The L2CAP buffer below additionally includes HIDP A3.
enum class FeatureCheck { Ok, Unsupported, Length, Crc };
inline FeatureCheck feature(const uint8_t *p, size_t n) {
    if (!p || n < 2) return FeatureCheck::Length;
    if (p[0] != 0xa3) return FeatureCheck::Unsupported;
    if (p[1] == 0xa3) return n == 50 ? FeatureCheck::Ok : FeatureCheck::Length;
    if (p[1] != 0x05) return FeatureCheck::Unsupported;
    if (n != 42) return FeatureCheck::Length;
    return valid_crc(p, n) ? FeatureCheck::Ok : FeatureCheck::Crc;
}
// Cached report starts at the report ID, with the HIDP A3 header removed.
// All 48 firmware bytes are data: never strip a supposed CRC or pad over it.
inline uint16_t firmware_usb(const uint8_t *cached, size_t n, uint8_t *out, size_t cap) {
    if (!cached || !out || n != 49 || cached[0] != 0xa3 || cap < 48) return 0;
    std::memcpy(out, cached + 1, 48);
    return 48;
}
inline void neutral(uint8_t *p) {
    std::memset(p, 0, kUsbPayload);
    p[0] = p[1] = p[2] = p[3] = 128;
    p[4] = 8;
    p[34] = p[38] = 0x80;
}
// Accept only the standard state packet; audio variants are deliberately not
// interpreted as input. Cap the USB history to its three complete touch frames.
inline bool input(const uint8_t *p, size_t n, uint8_t *usb) {
    if (!p || n != kBtInput || p[0] != 0xa1 || p[1] != 0x11 || !valid_crc(p, n)) return false;
    const uint8_t count = p[4 + 32];
    if (count > 4) return false;
    std::memcpy(usb, p + 4, kUsbPayload);
    if (count == 4) {
        usb[32] = 3;
        std::memcpy(usb + 33, p + 4 + 42, 27);
    }
    return true;
}
// A DS5-shaped view ONLY for existing OLED/calibration/remap consumers. Never
// send this view as a DS4 report. The original USB payload retains timestamps,
// status flags, IMU and touch history without DS5 reinterpretation.
inline void view(const uint8_t *p, uint8_t *v) {
    std::memset(v, 0, kUsbPayload);
    std::memcpy(v, p, 4);
    v[4] = p[7]; v[5] = p[8]; v[6] = p[6] >> 2;
    v[7] = p[4]; v[8] = p[5]; v[9] = p[6] & 3;
    std::memcpy(v + 15, p + 12, 12); // gyro and accelerometer
    v[32] = v[36] = 0x80;
    if (p[32] > 0 && p[32] <= 3) std::memcpy(v + 32, p + 34 + 9 * (p[32] - 1), 8);
    const uint8_t level = p[29] & 15;
    const bool cable = (p[29] & 0x10) != 0;
    v[52] = (level > 10 ? 10 : level) | (cable ? (level >= 11 ? 0x20 : 0x10) : 0);
    v[53] = (p[29] & 0x20) ? 1 : 0;
}
// Apply only controls and corrected gyro; untouched DS4 fields stay intact.
inline void apply_view(const uint8_t *v, uint8_t *p) {
    std::memcpy(p, v, 4);
    p[7] = v[4]; p[8] = v[5]; p[4] = v[7]; p[5] = v[8];
    p[6] = (p[6] & 0xfc) | (v[9] & 3);
    std::memcpy(p + 12, v + 15, 6);
}
inline bool active(const uint8_t *v) {
    for (int i = 0; i < 4; ++i) if (v[i] < 120 || v[i] > 140) return true;
    return v[4] || v[5] || v[7] != 8 || v[8] || (v[9] & 3);
}
// BT groups the gyro +/- words; USB interleaves them. Inputs may alias.
inline void calibration(const uint8_t *bt, uint8_t *usb) {
    uint8_t tmp[36]; std::memcpy(tmp, bt, 36);
    std::memcpy(usb, tmp, 36);
    std::memcpy(usb + 8, tmp + 12, 2);
    std::memcpy(usb + 10, tmp + 8, 2);
    std::memcpy(usb + 12, tmp + 14, 2);
    std::memcpy(usb + 14, tmp + 10, 2);
}
inline bool output(const uint8_t *payload, size_t n, uint8_t *bt) {
    if (!payload || n != 31) return false;
    std::memset(bt, 0, 78);
    bt[0] = 0x11; bt[1] = 0xc0; bt[2] = 0x20;
    std::memcpy(bt + 3, payload, 31);
    bt[3] &= 7; // rumble, lightbar, blink only: no audio/microphone activation
    return true; // bt_write adds A2 and CRC
}
} // namespace ds4
