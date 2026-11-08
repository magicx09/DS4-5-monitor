#pragma once

#include <cstdint>

#include "oled_zh_prop.h"
namespace oled_zh_data = oled_12px_proportional;

struct oled_zh_font_data {
    static constexpr int ascent = oled_zh_data::ascent;
    static constexpr int descent = oled_zh_data::descent;
    static constexpr uint32_t count = oled_zh_data::count;
    static constexpr const uint8_t *index = oled_zh_data::index;
    static constexpr const uint8_t *bitmap = oled_zh_data::bitmap;
};

namespace oled_zh {

inline uint32_t le32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline uint32_t next(const char *&s) {
    const uint8_t a = static_cast<uint8_t>(*s);
    // error: NOT-WORTH — stop at the terminator without advancing past it
    if (!a) return 0;
    ++s;
    if (a < 0x80) return a;
    const int n = a >= 0xc2 && a <= 0xdf ? 2 :
                  a >= 0xe0 && a <= 0xef ? 3 :
                  a >= 0xf0 && a <= 0xf4 ? 4 : 0;
    // error: SHOWN — invalid input consumes one byte and displays '?'
    if (!n) return '?';
    uint32_t cp = a & (0x7f >> n);
    // loop: HOT-PATH-OK — at most 3 continuation bytes per Unicode scalar
    for (int i = 0; i < n - 1; ++i) {
        const uint8_t b = static_cast<uint8_t>(s[i]);
        // error: SHOWN — truncated input displays '?'; never inspect beyond NUL
        if ((b & 0xc0) != 0x80) return '?';
        cp = (cp << 6) | (b & 0x3f);
    }
    // Reject overlong encodings, surrogate code points and values beyond Unicode.
    // error: SHOWN — display '?', leave continuation bytes for the next decode
    if (cp < (n == 2 ? 0x80u : n == 3 ? 0x800u : 0x10000u) ||
        (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) return '?';
    s += n - 1;
    return cp;
}

namespace detail {

template <typename Data>
inline const uint8_t *glyph(uint32_t cp) {
    int lo = 0, hi = static_cast<int>(Data::count) - 1;
    // loop: HOT-PATH-OK — binary search, at most 9 probes for the OLED subsets
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const uint8_t *rec = Data::index + mid * 13;
        const uint32_t at = le32(rec);
        if (at == cp) return rec;
        if (at < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    // error: SHOWN — visible_glyph substitutes '?' for an absent code point
    return nullptr;
}

template <typename Data>
inline const uint8_t *visible_glyph(uint32_t cp) {
    const uint8_t *g = glyph<Data>(cp);
    // error: SHOWN — missing code points use the visible '?' glyph
    return g ? g : glyph<Data>('?');
}

template <typename Data>
inline int width(const char *s) {
    int w = 0;
    // loop: HOT-PATH-OK — Status strings are at most 31 UTF-8 bytes
    while (*s) {
        const uint8_t *g = visible_glyph<Data>(next(s));
        w += g ? g[12] : 6;
    }
    return w;
}

template <typename Data, typename Pixel>
void draw(int x, int y, const char *s, int clip_x0, int clip_y0,
          int clip_x1, int clip_y1, Pixel pixel) {
    // loop: HOT-PATH-OK — positive glyph advances; at most 128 glyphs in a 128px screen
    while (*s && x < clip_x1) {
        const uint8_t *g = visible_glyph<Data>(next(s));
        if (!g) { x += 6; continue; }
        const int w = g[8], h = g[9];
        const int left = x + static_cast<int8_t>(g[10]);
        const int top = y + Data::ascent - static_cast<int8_t>(g[11]) - h;
        const uint8_t *bits = Data::bitmap + le32(g + 4);
        const int stride = (w + 7) / 8;
        // Clip once per glyph instead of testing every pixel outside the cell.
        const int row0 = clip_y0 > top ? clip_y0 - top : 0;
        const int row1 = clip_y1 < top + h ? clip_y1 - top : h;
        const int col0 = clip_x0 > left ? clip_x0 - left : 0;
        const int col1 = clip_x1 < left + w ? clip_x1 - left : w;
        // loop: HOT-PATH-OK — at most 16 bitmap rows per glyph in this subset
        for (int row = row0; row < row1; ++row) {
            const int py = top + row;
            // loop: HOT-PATH-OK — at most 12 bitmap columns per glyph
            for (int col = col0; col < col1; ++col) {
                const int px = left + col;
                if (bits[row * stride + col / 8] & (0x80 >> (col % 8))) pixel(px, py);
            }
        }
        x += g[12];
    }
}

} // namespace detail

inline const uint8_t *glyph(uint32_t cp) {
    return detail::glyph<oled_zh_font_data>(cp);
}

inline const uint8_t *visible_glyph(uint32_t cp) {
    return detail::visible_glyph<oled_zh_font_data>(cp);
}

inline int width(const char *s) {
    return detail::width<oled_zh_font_data>(s);
}

template <typename Pixel>
void draw(int x, int y, const char *s, int clip_x0, int clip_y0,
          int clip_x1, int clip_y1, Pixel pixel) {
    detail::draw<oled_zh_font_data>(x, y, s, clip_x0, clip_y0, clip_x1, clip_y1, pixel);
}

} // namespace oled_zh
