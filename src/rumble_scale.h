#pragma once
#include <cstdint>
// Shared by forwarded DS4/DS5 reports and firmware-originated DS5 state.
inline uint8_t scale_rumble(uint8_t value, float gain) {
    const int scaled = static_cast<int>(value * gain + 0.5f);
    return static_cast<uint8_t>(scaled < 0 ? 0 : scaled > 255 ? 255 : scaled);
}
