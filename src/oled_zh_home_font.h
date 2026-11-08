// Compatibility alias: every OLED screen now uses the same Fusion Pixel
// 12px Proportional subset.
#pragma once
#include "oled_zh_font.h"

namespace oled_zh_home_data = oled_zh_data;
using oled_zh_home_font_data = oled_zh_font_data;

namespace oled_zh_home {

using oled_zh::glyph;
using oled_zh::width;

template <typename Pixel>
void draw(int x, int y, const char *s, int clip_x0, int clip_y0,
          int clip_x1, int clip_y1, Pixel pixel) {
    oled_zh::draw(x, y, s, clip_x0, clip_y0, clip_x1, clip_y1, pixel);
}

} // namespace oled_zh_home
