// Chinese Status page. Pure drawing shared by the firmware and host tests.
#pragma once
#include <cstdio>
#include <cstring>
#include "oled_pad.h"
#include "oled_toast.h"
#include "oled_zh_home_font.h"
#include "pair_prompt.h"
#include "signal_screen.h"

namespace home_zh {
constexpr int font_offset(int cell) {
    return cell == 12 ? -3 : cell == 10 ? -2 : 0;
}

template <typename C>
void line(C &c, int y, const char *s, int cell = OLED_ROW_HEIGHT) {
    c.zh_small_text(6, y + font_offset(cell), s, 6, y, 128, y + cell);
}

inline const char *title(bool connected, uint8_t power, uint32_t seconds) {
    if (!connected) return seconds ? "配对中" : "手柄状态";
    const uint8_t state = power >> 4;
    if (state == 1) return "充电中";
    if (state == 2) return "充电完成";
    if (state >= 10) return "电量异常";
    return "已连接";
}

// Connected heading: replace the version/model text with the live RSSI bar.
// Same relative RSSI scale as the signal page; unknown leaves an empty frame.
// x=120..127 remains available for the existing connection icon.
template <typename C>
void signal_bar(C &c, bool valid, int8_t rssi) {
    c.outline(60, 2, 56, 8);
    const int fill = valid ? signal_screen::bar_fill(rssi) * 52 / signal_screen::kBarInner : 0;
    if (fill > 0) c.filled(62, 4, fill, 4);
}

// The complete controller glyph occupies y=30..60. Connected metadata uses
// a 12px heading, 7px MAC text and the original 8px battery row shifted 4px.
// Keeping the MAC/digits compact preserves all six octets and both stick boxes.
template <typename C>
struct BatteryCanvas {
    C &canvas;
    void outline(int x, int y, int w, int h) { canvas.outline(x, y + 4, w, h); }
    void filled(int x, int y, int w, int h) { canvas.filled(x, y + 4, w, h); }
};

struct Charge {
    bool charging, valid, provisional;
    int minutes;
};

template <typename C>
void connected_details(C &c, const uint8_t address[6], uint8_t power, Charge eta) {
    char mac[18];
    std::snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                  address[0], address[1], address[2], address[3], address[4], address[5]);
    c.text(6, 13, mac);
    int percent = (power & 15) * 10;
    if (percent > 100) percent = 100;
    const uint8_t state = power >> 4;
    const char marker = state == 1 ? '+' : state == 2 ? '*' : state >= 10 ? '!' : ' ';
    char value[16];
    std::snprintf(value, sizeof(value), "%3d%%%c", percent, marker);
    c.text(oled_pad::kBattTextX, 22, value);
    BatteryCanvas<C> shifted{c};
    oled_pad::battery(shifted, percent);
    if (eta.charging) {
        const int minutes = eta.minutes < 0 ? 0 : eta.minutes > 999 ? 999 : eta.minutes;
        if (eta.valid) std::snprintf(value, sizeof(value), "~%dm%s", minutes, eta.provisional ? "?" : "");
        else std::snprintf(value, sizeof(value), "~--m");
        c.text(oled_pad::kEtaX, 22, value);
    }
}

template <typename C>
void disconnected_details(C &c, bool paired, uint32_t seconds) {
    constexpr int cell = OLED_ROW_HEIGHT;
    if (seconds) {
        line(c, cell, "长按分享键+PS");
        char time[24];
        const unsigned bounded = seconds > 599 ? 599 : seconds;
        std::snprintf(time, sizeof(time), "剩余 %u:%02u", bounded / 60, bounded % 60);
        line(c, cell * 2, time);
        const int y = cell * 3 + (cell - 8) / 2;
        c.outline(6, y, 122, 8);
        const int fill = pair_prompt::bar_fill(seconds);
        if (fill) c.filled(8, y + 2, fill, 4);
    } else {
        line(c, cell, paired ? "按 PS 连接" : "未配对手柄");
        line(c, cell * 2, "长按 KEY0 两秒");
        line(c, cell * 3, "开始配对");
    }
}

// Translate existing notice tokens at the display boundary. Bluetooth/storage
// code and the default English build retain their existing messages.
inline const char *notice(const char *msg) {
    if (std::strcmp(msg, oled_toast::kBrightness) == 0) return "亮度保存失败";
    if (std::strcmp(msg, oled_toast::kLightbar) == 0) return "灯条保存失败";
    if (std::strcmp(msg, oled_toast::kSlot) == 0) return "槽位保存失败";
    if (std::strcmp(msg, oled_toast::kPair) == 0) return "配对保存失败";
    if (std::strcmp(msg, oled_toast::kFlip) == 0) return "翻转保存失败";
    if (std::strcmp(msg, oled_toast::kFlashSmall) == 0) return "闪存不足，无法保存";
    constexpr const char *slots[] = {"已配对槽位0", "已配对槽位1", "已配对槽位2", "已配对槽位3"};
    // loop: HOT-PATH-OK — at most 4 slot notices at 10 Hz while a toast is visible
    for (int i = 0; i < 4; ++i)
        if (std::strcmp(msg, oled_toast::kPairedInSlot[i]) == 0) return slots[i];
    return msg;
}
} // namespace home_zh
