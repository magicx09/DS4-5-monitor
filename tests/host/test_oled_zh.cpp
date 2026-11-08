// Real firmware renderer, compiled separately for all four font/row variants.
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include "oled_zh_font.h"
#include "home_zh.h"
#include "identify_screen.h"
#include "oled_font.h"
#include "settings_menu.h"
#include "signal_screen.h"
#include "ui_zh.h"
#include "test.h"

namespace {
using namespace settings_menu;

struct TextCall {
    std::string text;
    int y0, y1;
    int clipped = 0;
    bool small = false;
};

struct Canvas {
    bool pixels[64][128]{};
    int off_screen = 0;
    std::vector<TextCall> calls;
    void px(int x, int y, bool on = true) {
        if (x < 0 || x >= 128 || y < 0 || y >= 64) { ++off_screen; return; }
        pixels[y][x] = on;
    }
    void invert(int x, int y, int w, int h) {
        for (int py = y; py < y + h; ++py)
            for (int px_x = x; px_x < x + w; ++px_x) {
                if (px_x < 0 || px_x >= 128 || py < 0 || py >= 64) ++off_screen;
                else pixels[py][px_x] = !pixels[py][px_x];
            }
    }
    void filled(int x, int y, int w, int h) {
        for (int py = y; py < y + h; ++py)
            for (int px_x = x; px_x < x + w; ++px_x) px(px_x, py);
    }
    void outline(int x, int y, int w, int h) {
        for (int dx = 0; dx < w; ++dx) { px(x + dx, y); px(x + dx, y + h - 1); }
        for (int dy = 0; dy < h; ++dy) { px(x, y + dy); px(x + w - 1, y + dy); }
    }
    void text(int x, int y, const char *s) {
        for (; *s; ++s, x += 6) {
            CHECK(*s >= 32 && *s <= 126);
            if (*s < 32 || *s > 126) continue;
            for (int col = 0; col < 5; ++col)
                for (int row = 0; row < 7; ++row)
                    if (kFont5x7[*s - 32][col] & (1 << row)) px(x + col, y + row);
        }
    }
    int zh_width(const char *s) { return oled_zh::width(s); }
    void zh_text(int x, int y, const char *s, int x0, int y0, int x1, int y1) {
        TextCall call{s, y0, y1};
        // Unclipped ink is the independent layout oracle: no valid menu text
        // may lose pixels at a label/value boundary or at the cell edge.
        oled_zh::draw(x, y, s, -4096, -4096, 4096, 4096, [&](int px_x, int py) {
            if (px_x < x0 || px_x >= x1 || py < y0 || py >= y1) ++call.clipped;
        });
        oled_zh::draw(x, y, s, x0, y0, x1, y1, [&](int px_x, int py) { px(px_x, py); });
        calls.push_back(call);
    }
    int zh_small_width(const char *s) { return oled_zh_home::width(s); }
    void zh_small_text(int x, int y, const char *s, int x0, int y0, int x1, int y1) {
        TextCall call{s, y0, y1, 0, true};
        oled_zh_home::draw(x, y, s, -4096, -4096, 4096, 4096, [&](int px_x, int py) {
            if (px_x < x0 || px_x >= x1 || py < y0 || py >= y1) ++call.clipped;
        });
        oled_zh_home::draw(x, y, s, x0, y0, x1, y1, [&](int px_x, int py) { px(px_x, py); });
        calls.push_back(call);
    }
    bool contains(const char *s, int y0 = -1) const {
        for (const auto &call : calls)
            if (call.text == s && (y0 < 0 || call.y0 == y0)) return true;
        return false;
    }
    void dump(const std::string &name) const {
        const char *dir = std::getenv("OLED_ZH_DUMP");
        if (!dir) return;
        std::ofstream out(std::string(dir) + "/" + name + ".pgm", std::ios::binary);
        out << "P5\n128 64\n255\n";
        for (const auto &row : pixels)
            for (bool on : row) out.put(on ? char(255) : char(0));
        CHECK(out.good());
    }
};

Config_body body() {
    Config_body b{};
    b.haptics_gain = b.haptics_gain_right = 1.0f;
    b.speaker_volume = -20.0f;
    b.inactive_time = 15;
    b.audio_buffer_length = kAudioBufferMin;
    return b;
}

void check_glyphs(const char *s) {
    while (*s) {
        const auto cp = oled_zh::next(s);
        if (!oled_zh::glyph(cp)) {
            std::fprintf(stderr, "Missing glyph U+%04x\n", cp);
            CHECK(false);
        }
    }
}

void check_home_glyphs(const char *s) {
    while (*s) {
        const auto cp = oled_zh::next(s);
        if (!oled_zh_home::glyph(cp)) {
            std::fprintf(stderr, "Missing compact glyph U+%04x\n", cp);
            CHECK(false);
        }
    }
}

void check_canvas(const Canvas &c) {
    CHECK_EQ(c.off_screen, 0);
    for (const auto &call : c.calls) {
        if (call.small) check_home_glyphs(call.text.c_str());
        else check_glyphs(call.text.c_str());
        if (call.clipped) std::fprintf(stderr, "Clipped text: %s (%d pixels)\n", call.text.c_str(), call.clipped);
        CHECK_EQ(call.clipped, 0);
    }
    // The real driver adds chrome in x=0..5, which the menu must not touch.
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 6; ++x) CHECK(!c.pixels[y][x]);
}

void check_compact_line(const char *s) {
    Canvas c;
    c.zh_small_text(ui_zh::kX, home_zh::font_offset(ui_zh::kLineH), s, ui_zh::kX, 0,
                    ui_zh::kX + ui_zh::kMaxPx, ui_zh::kLineH);
    check_canvas(c);
    CHECK(oled_zh_home::width(s) <= ui_zh::kMaxPx);
}

void preview_line(Canvas &c, int x, int y, const char *s, int x1 = 120) {
    c.zh_small_text(x, y + home_zh::font_offset(ui_zh::kLineH), s,
                    x, y, x1, y + ui_zh::kLineH);
}
} // namespace

TEST(zh_utf8_scalar_validation_and_progress) {
    for (const auto &s : {std::string("\xc0\xaf"), std::string("\xe0\x80\x80"),
                          std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"),
                          std::string("\x80"), std::string("\xf5\x80\x80\x80")}) {
        const char *p = s.c_str();
        CHECK_EQ(oled_zh::next(p), static_cast<uint32_t>('?'));
        CHECK(p == s.c_str() + 1);
    }
    const char *p = "\xf0\x9f\x98\x80"; // valid scalar, absent from the subset
    CHECK_EQ(oled_zh::next(p), 0x1f600u);
    CHECK_EQ(*p, 0);
    const char empty[] = "";
    p = empty;
    CHECK_EQ(oled_zh::next(p), 0u);
    CHECK(p == empty);
}

TEST(zh_utf8_exact_truncated_buffers_are_bounded) {
    // Exact allocations let ASan catch even a one-byte read past the NUL.
    for (const std::string &s : {std::string("\xc2"), std::string("\xe4"), std::string("\xe4\xb8"),
                                std::string("\xf0"), std::string("\xf0\x9f"), std::string("\xf0\x9f\x98")}) {
        std::vector<char> bytes(s.begin(), s.end());
        bytes.push_back(0);
        bytes.shrink_to_fit();
        const char *p = bytes.data();
        while (*p) {
            CHECK_EQ(oled_zh::next(p), static_cast<uint32_t>('?'));
            CHECK(p <= bytes.data() + s.size());
        }
        Canvas c;
        c.zh_text(0, 0, bytes.data(), 0, 0, 128, 64);
        CHECK_EQ(c.off_screen, 0);
    }
}

TEST(zh_unknown_character_draws_visible_fallback) {
    bool expected[64][128]{}, actual[64][128]{};
    oled_zh::draw(0, 0, "?", 0, 0, 128, 64, [&](int x, int y) { expected[y][x] = true; });
    oled_zh::draw(0, 0, "\xf0\x9f\x98\x80", 0, 0, 128, 64, [&](int x, int y) { actual[y][x] = true; });
    CHECK(std::memcmp(actual, expected, sizeof(actual)) == 0);
    CHECK_EQ(oled_zh::width("\xf0\x9f\x98\x80"), oled_zh::width("?"));
}

TEST(zh_font_index_and_bitmap_are_valid) {
    CHECK_EQ(sizeof(oled_zh_data::index), oled_zh_data::count * 13u);
    uint32_t previous = 0;
    for (uint32_t i = 0; i < oled_zh_data::count; ++i) {
        const auto *g = oled_zh_data::index + i * 13;
        const uint32_t cp = oled_zh::le32(g);
        CHECK(i == 0 || cp > previous);
        previous = cp;
        CHECK(oled_zh::glyph(cp) == g);
        CHECK(oled_zh::le32(g + 4) + ((g[8] + 7) / 8) * g[9] <= sizeof(oled_zh_data::bitmap));
        CHECK(g[12] > 0);
    }
    check_glyphs("XY×△○✖□已保存保存失败已恢复默认恢复失败配对已清空清空失败");
}

TEST(zh_home_font_index_and_bitmap_are_valid) {
    CHECK_EQ(sizeof(oled_zh_home_data::index), oled_zh_home_data::count * 13u);
    CHECK_EQ(oled_zh_home_data::count, 327u);
    uint32_t previous = 0;
    for (uint32_t i = 0; i < oled_zh_home_data::count; ++i) {
        const auto *g = oled_zh_home_data::index + i * 13;
        const uint32_t cp = oled_zh::le32(g);
        CHECK(i == 0 || cp > previous);
        previous = cp;
        CHECK(oled_zh_home::glyph(cp) == g);
        CHECK(oled_zh::le32(g + 4) + ((g[8] + 7) / 8) * g[9] <= sizeof(oled_zh_home_data::bitmap));
        CHECK(g[9] <= 12);
    }
    check_home_glyphs("手柄状态未配对手柄配对中长按分享键+PS剩余按 PS 连接长按 KEY0 两秒开始配对"
                      "已连接充电中充电完成电量异常亮度保存失败灯条槽位配对闪存不足，无法保存已配对槽位0123"
                      "DS5桥接器");
}

TEST(zh_every_secondary_screen_string_fits_the_compact_face) {
    for (const char *s : {ui_zh::kNoController, ui_zh::kSlotsTitle, ui_zh::kConnected,
                          ui_zh::kDisconnected, ui_zh::kEmpty, ui_zh::kSlotHelp,
                          ui_zh::kSlotConnect, ui_zh::kSlotFull, ui_zh::kSwitched,
                          ui_zh::kSwitchFail, ui_zh::kWiped, ui_zh::kWipeFail,
                          ui_zh::kLightbarTitle, ui_zh::kLightbarSaveHint,
                          ui_zh::kLightbarFooter, ui_zh::kTriggersTitle,
                          ui_zh::kTriggerFooter, ui_zh::kGyroTitle, ui_zh::kTouchpadTitle,
                          ui_zh::kDiagTitle, ui_zh::kCpuTitle, ui_zh::kRssiTitle,
                          ui_zh::kInRange, ui_zh::kUnknown, ui_zh::kVuTitle,
                          ui_zh::kVuFooter, ui_zh::kIdentifyTitle, ui_zh::kFault1,
                          ui_zh::kFault2, ui_zh::kFault3, ui_zh::kBootBoard}) {
        check_compact_line(s);
    }

    char line[128];
    for (int slot = 0; slot < 4; ++slot) {
        for (bool connected : {false, true}) {
            ui_zh::slots_header(line, sizeof(line), slot, connected);
            check_compact_line(line);
        }
    }
    for (const char *s : {ui_zh::kSlotHelp, ui_zh::kSlotConnect, ui_zh::kSlotFull,
                          ui_zh::kSwitched, ui_zh::kSwitchFail, ui_zh::kWiped,
                          ui_zh::kWipeFail, static_cast<const char *>(nullptr)}) {
        for (bool connected : {false, true}) {
            for (bool full : {false, true}) check_compact_line(ui_zh::slots_footer(s, connected, full));
        }
    }

    for (int mode = 0; mode <= 9; ++mode) {
        check_compact_line(ui_zh::lightbar_tag(mode));
        check_compact_line(ui_zh::lightbar_hint(mode));
    }
    for (int preset = 0; preset <= 7; ++preset) {
        ui_zh::trigger_mode(line, sizeof(line), preset);
        check_compact_line(line);
    }
    for (int fingers = 0; fingers <= 10; ++fingers) {
        ui_zh::touchpad_fingers(line, sizeof(line), fingers);
        check_compact_line(line);
    }

    ui_zh::diag_uptime(line, sizeof(line), UINT64_MAX);
    check_compact_line(line);
    check_compact_line(ui_zh::diag_bt_state(false));
    check_compact_line(ui_zh::diag_bt_state(true));
    ui_zh::diag_host02(line, sizeof(line), UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_trig_tx(line, sizeof(line), UINT32_MAX, UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_drops(line, sizeof(line), UINT32_MAX, UINT32_MAX, UINT32_MAX);
    check_compact_line(line);
    for (const char *label : {"蓝牙输入31:", "USB音频:", "蓝牙输出32:", "麦克风输入:"}) {
        ui_zh::diag_rate(line, sizeof(line), label, UINT32_MAX);
        check_compact_line(line);
    }
    ui_zh::diag_mic_dec(line, sizeof(line), INT32_MIN, UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_mic_plc(line, sizeof(line), UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_c1(line, sizeof(line), UINT32_MAX, UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_pushes(line, sizeof(line), UINT32_MAX, UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_bt_drop(line, sizeof(line), UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_mic_lost(line, sizeof(line), UINT32_MAX, UINT32_MAX, UINT32_MAX);
    check_compact_line(line);
    ui_zh::diag_usb_ovr(line, sizeof(line), UINT32_MAX);
    check_compact_line(line);

    ui_zh::cpu_set(line, sizeof(line), 320000);
    check_compact_line(line);
    ui_zh::cpu_real(line, sizeof(line), 320000);
    check_compact_line(line);
    ui_zh::cpu_vcore(line, sizeof(line), 1300);
    check_compact_line(line);
    ui_zh::cpu_vcode(line, sizeof(line), 15);
    check_compact_line(line);
    ui_zh::cpu_temp(line, sizeof(line), -400);
    check_compact_line(line);
    ui_zh::cpu_temp(line, sizeof(line), 1250);
    check_compact_line(line);

    for (const char *label : {ui_zh::rssi_label(0), ui_zh::rssi_label(-5),
                              ui_zh::rssi_label(-15), ui_zh::rssi_label(-25),
                              ui_zh::rssi_label(-128)}) check_compact_line(label);
    ui_zh::rssi_value(line, sizeof(line), false, 0);
    check_compact_line(line);
    ui_zh::rssi_value(line, sizeof(line), true, -128);
    check_compact_line(line);
    ui_zh::rssi_link(line, sizeof(line), false, 0);
    check_compact_line(line);
    ui_zh::rssi_link(line, sizeof(line), true, -128);
    check_compact_line(line);

    ui_zh::vu_speaker(line, sizeof(line), 255);
    check_compact_line(line);
    ui_zh::vu_haptic(line, sizeof(line), 255);
    check_compact_line(line);
    ui_zh::identify_slot(line, sizeof(line), 255);
    check_compact_line(line);
}

TEST(zh_secondary_screen_previews_use_complete_glyphs) {
    char line[128];

    {
        Canvas c;
        ui_zh::slots_header(line, sizeof(line), 0, true);
        preview_line(c, 6, 0, line);
        for (int slot = 0; slot < 4; ++slot) {
            std::snprintf(line, sizeof(line), "%s%d%s %s", slot == 0 ? ">" : " ", slot,
                          slot == 0 ? "*" : " ", slot == 0 ? "FFAB01123456" : ui_zh::kEmpty);
            preview_line(c, 6, 12 + slot * 12, line);
        }
        check_canvas(c);
        c.dump("screen-slots");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kLightbarTitle);
        preview_line(c, 126 - oled_zh_home::width(ui_zh::lightbar_tag(6)),
                     0, ui_zh::lightbar_tag(6), 128);
        preview_line(c, 6, 12, "红200");
        preview_line(c, 48, 12, "绿080");
        preview_line(c, 90, 12, "蓝255");
        preview_line(c, 6, 36, ui_zh::kLightbarSaveHint);
        preview_line(c, 6, 48, ui_zh::lightbar_hint(6));
        check_canvas(c);
        c.dump("screen-lightbar");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kTriggersTitle);
        ui_zh::trigger_mode(line, sizeof(line), 2);
        preview_line(c, 6, 12, line);
        preview_line(c, 6, 24, "L2:128  R2:224");
        c.outline(6, 35, 56, 9);
        c.outline(72, 35, 56, 9);
        c.filled(8, 37, 26, 5);
        c.filled(74, 37, 45, 5);
        preview_line(c, 6, 52, ui_zh::kTriggerFooter);
        check_canvas(c);
        c.dump("screen-triggers");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kGyroTitle);
        preview_line(c, 6, 12, "X+12 Y-34 Z+56");
        c.outline(44, 22, 40, 40);
        c.filled(62, 40, 3, 3);
        check_canvas(c);
        c.dump("screen-gyro");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kTouchpadTitle);
        c.outline(8, 12, 116, 30);
        c.filled(40, 23, 3, 3);
        ui_zh::touchpad_fingers(line, sizeof(line), 1);
        preview_line(c, 6, 46, line);
        check_canvas(c);
        c.dump("screen-touchpad");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kDiagTitle);
        preview_line(c, 6, 12, "运行 12:34:56");
        preview_line(c, 6, 24, ui_zh::diag_bt_state(true));
        ui_zh::diag_rate(line, sizeof(line), "蓝牙输入31:", 1000);
        preview_line(c, 6, 36, line);
        ui_zh::diag_drops(line, sizeof(line), 1, 2, 3);
        preview_line(c, 6, 48, line);
        check_canvas(c);
        c.dump("screen-diag");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kCpuTitle);
        ui_zh::cpu_set(line, sizeof(line), 150000);
        preview_line(c, 6, 12, line);
        ui_zh::cpu_real(line, sizeof(line), 150000);
        preview_line(c, 6, 24, line);
        ui_zh::cpu_vcore(line, sizeof(line), 1100);
        preview_line(c, 6, 36, line);
        ui_zh::cpu_temp(line, sizeof(line), 421);
        preview_line(c, 6, 48, line);
        check_canvas(c);
        c.dump("screen-cpu");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kRssiTitle);
        ui_zh::rssi_value(line, sizeof(line), true, -8);
        preview_line(c, 6, 12, line);
        c.outline(6, 36, signal_screen::kBarW, signal_screen::kBarH);
        c.filled(8, 38, 92, signal_screen::kBarH - 4);
        ui_zh::rssi_link(line, sizeof(line), true, -8);
        preview_line(c, 6, 48, line);
        check_canvas(c);
        c.dump("screen-rssi");
    }
    {
        Canvas c;
        preview_line(c, 6, 0, ui_zh::kVuTitle);
        ui_zh::vu_speaker(line, sizeof(line), 196);
        preview_line(c, 6, 12, line);
        c.outline(56, 12, 68, 8);
        c.filled(58, 14, 49, 4);
        ui_zh::vu_haptic(line, sizeof(line), 128);
        preview_line(c, 6, 28, line);
        c.outline(56, 28, 68, 8);
        c.filled(58, 30, 32, 4);
        preview_line(c, 6, 44, ui_zh::kVuFooter);
        check_canvas(c);
        c.dump("screen-vu");
    }
    {
        Canvas c;
        const int tx = (128 - oled_zh_home::width(ui_zh::kIdentifyTitle)) / 2;
        preview_line(c, tx, 14, ui_zh::kIdentifyTitle, 124);
        identify::text_scaled(c, identify::centered_x("PICO2W", 1), 32, "PICO2W", 1);
        ui_zh::identify_slot(line, sizeof(line), 2);
        preview_line(c, (128 - oled_zh_home::width(line)) / 2, 46, line, 124);
        c.dump("screen-identify");
    }
    {
        Canvas c;
        preview_line(c, 6, 16, ui_zh::kFault1, 122);
        preview_line(c, 6, 30, ui_zh::kFault2, 122);
        preview_line(c, 6, 44, ui_zh::kFault3, 122);
        check_canvas(c);
        c.dump("screen-fault");
    }
}

TEST(zh_all_values_and_pages_keep_every_ink_pixel) {
    const Config_body saved = body();
    for (int group = 0; group < kNumGroups; ++group) {
        Nav nav;
        nav.in_group = true;
        nav.group = static_cast<uint8_t>(group);
        for (int row = 0; row < nav.rows(); ++row) {
            nav.row = static_cast<uint8_t>(row);
            const auto &it = kItems[nav.item()];
            Config_body local = saved;
            uint8_t power = POWER_FLAGS_DEFAULT;
            for (int dir : {-1, 1}) {
                for (int step = 0; step < 260; ++step) {
                    Canvas c;
                    draw(c, nav, View{local, power, saved, POWER_FLAGS_DEFAULT, true, nullptr});
                    bool saw_label = false;
                    for (const auto &call : c.calls) {
                        if (call.text == it.label) {
                            CHECK(!call.small);
                            saw_label = true;
                        }
                    }
                    CHECK(saw_label);
                    check_canvas(c);
                    adjust(it, local, power, dir);
                }
            }
            Canvas c;
            draw(c, nav, View{saved, POWER_FLAGS_DEFAULT, saved, POWER_FLAGS_DEFAULT, false, nullptr});
            c.dump("group-" + std::to_string(group) + "-row-" + std::to_string(row));
        }
    }
    for (int group = 0; group < kNumGroups; ++group) {
        Nav nav;
        nav.group = static_cast<uint8_t>(group);
        Canvas c;
        draw(c, nav, View{saved, POWER_FLAGS_DEFAULT, saved, POWER_FLAGS_DEFAULT, true, nullptr});
        check_canvas(c);
        CHECK(c.contains(kGroups[group].name));
        c.dump("menu-" + std::to_string(group));
    }
}

TEST(zh_reset_hint_is_visible_in_both_row_heights) {
    const auto b = body();
    for (int row : {0, 1}) {
        Nav nav;
        nav.in_group = true;
        nav.group = 5;
        nav.row = row;
        Canvas c;
        draw(c, nav, View{b, 0, b, 0, false, nullptr});
        CHECK(c.contains(kItems[nav.item()].hint, OLED_SETTINGS_ROW_HEIGHT == 14 ? 50 : 48));
        check_canvas(c);
    }
}

TEST(zh_status_keeps_selected_item_visible) {
    const auto b = body();
    for (int group = 0; group < kNumGroups; ++group) {
        Nav nav;
        nav.in_group = true;
        nav.group = static_cast<uint8_t>(group);
        for (int row = 0; row < nav.rows(); ++row) {
            nav.row = row;
            for (const char *status : {"已保存", "保存失败", "已恢复默认", "恢复失败", "配对已清空", "清空失败"}) {
                Canvas c;
                draw(c, nav, View{b, 0, b, 0, false, status});
                CHECK(c.contains(kItems[nav.item()].label));
                CHECK(c.contains(status, OLED_SETTINGS_ROW_HEIGHT == 14 ? 50 : 48));
                for (const auto &call : c.calls)
                    if (call.text == kItems[nav.item()].label)
                        CHECK(call.y1 <= (OLED_SETTINGS_ROW_HEIGHT == 14 ? 42 : 48));
                check_canvas(c);
                if (group == 1 && row == 9 && std::strcmp(status, "已保存") == 0) c.dump("saved-last-row");
            }
        }
    }
}

TEST(zh_changed_value_has_dark_window_and_lit_cap) {
    const auto saved = body();
    auto local = saved;
    local.speaker_volume = -100.0f;
    Nav nav;
    nav.in_group = true;
    nav.group = 1;
    Canvas c;
    draw(c, nav, View{local, 0, saved, 0, true, nullptr});
    CHECK(c.pixels[OLED_SETTINGS_ROW_HEIGHT][123]);
    CHECK(!c.pixels[OLED_SETTINGS_ROW_HEIGHT][121]);
    c.dump("dirty-value");
}

TEST(zh_clipping_matches_unclipped_reference) {
    // Every crop of mixed-width glyphs, including negative draw positions.
    for (int x : {-20, -1, 0, 7, 120}) {
        for (int y : {-15, -1, 0, 50, 63}) {
            bool reference[64][128]{}, crop[64][128]{};
            oled_zh::draw(x, y, "X屏幕Y×✖g", 0, 0, 128, 64,
                          [&](int px, int py) { reference[py][px] = true; });
            oled_zh::draw(x, y, "X屏幕Y×✖g", 9, 4, 119, 59,
                          [&](int px, int py) { CHECK(px >= 9 && px < 119 && py >= 4 && py < 59); crop[py][px] = true; });
            for (int py = 0; py < 64; ++py)
                for (int px = 0; px < 128; ++px)
                    CHECK(crop[py][px] == (reference[py][px] && px >= 9 && px < 119 && py >= 4 && py < 59));
        }
    }
}

TEST(zh_home_disconnected_states_and_pair_countdowns_fit) {
    for (bool paired : {false, true}) {
        for (uint32_t secs : {0u, 1u, 9u, 59u, 60u, 119u, 120u, 599u, 600u, 0xffffffffu}) {
            Canvas c;
            const char *title = home_zh::title(false, 0, secs);
            c.zh_small_text(6, home_zh::font_offset(OLED_ROW_HEIGHT), title, 6, 0, 59, OLED_ROW_HEIGHT);
            c.text(60, (OLED_ROW_HEIGHT - 7) / 2, "home11");
            home_zh::disconnected_details(c, paired, secs);
            check_canvas(c);
            if (secs) {
                CHECK(c.contains("配对中"));
                CHECK(c.contains("长按分享键+PS"));
            } else {
                CHECK(c.contains(paired ? "按 PS 连接" : "未配对手柄"));
                CHECK(c.contains("长按 KEY0 两秒"));
                CHECK(c.contains("开始配对"));
            }
            if (secs == 0 || secs == 120)
                c.dump(secs ? "home-pairing" : paired ? "home-reconnect" : "home-unpaired");
        }
    }
}

TEST(zh_home_connected_metadata_keeps_controller_graphics) {
    const uint8_t address[] = {0xff, 0xab, 0x01, 0x12, 0x34, 0x56};
    for (int power = 0; power <= 255; ++power) {
        for (bool edge : {false, true}) {
            Canvas c;
            const char *title = home_zh::title(true, power, 0);
            c.zh_small_text(6, home_zh::font_offset(12), title, 6, 0, 59, 12);
            home_zh::signal_bar(c, true, -12);
            home_zh::connected_details(c, address, power, {(power >> 4) == 1, true, true, 999});
            for (int y = oled_pad::kTop; y < 64; ++y)
                for (int x = 0; x < 128; ++x) CHECK(!c.pixels[y][x]);
            uint8_t report[64]{};
            report[0] = report[1] = report[2] = report[3] = 128;
            report[4] = 64; report[5] = 192; report[7] = 8;
            oled_pad::draw(c, report, edge);
            check_canvas(c);
            if (power == 0x17 && !edge) c.dump("home-charging");
            if (power == 0x0a && !edge) c.dump("home-connected");
            if (power == 0x2a && !edge) c.dump("home-full");
            if (power == 0xa0 && !edge) c.dump("home-battery-error");
        }
    }
    CHECK(std::strcmp(home_zh::title(true, 0x17, 0), "充电中") == 0);
    CHECK(std::strcmp(home_zh::title(true, 0x2a, 0), "充电完成") == 0);
    CHECK(std::strcmp(home_zh::title(true, 0xa0, 0), "电量异常") == 0);
    CHECK(std::strcmp(home_zh::title(true, 0x08, 0), "已连接") == 0);
    // The bar must never overwrite the title, connection icon or metadata.
    // Check every possible HCI value, including 0 (a real, full reading).
    int previous_ink = -1;
    for (int rssi = -128; rssi <= 127; ++rssi) {
        Canvas c;
        home_zh::signal_bar(c, true, static_cast<int8_t>(rssi));
        int ink = 0;
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) {
            if (!c.pixels[y][x]) continue;
            CHECK(x >= 60 && x < 116 && y >= 2 && y < 10);
            if (x >= 62 && x < 114 && y >= 4 && y < 8) ++ink;
        }
        CHECK(ink >= previous_ink);
        if (rssi <= -30) CHECK_EQ(ink, 0);
        if (rssi >= 0) CHECK_EQ(ink, 52 * 4);
        previous_ink = ink;
        check_canvas(c);
    }
    Canvas unknown;
    home_zh::signal_bar(unknown, false, 0);
    CHECK(unknown.pixels[2][60]);
    for (int y = 4; y < 8; ++y) for (int x = 62; x < 114; ++x)
        CHECK(!unknown.pixels[y][x]);
}

TEST(zh_home_notices_and_boot_title_have_all_glyphs) {
    for (const char *msg : {oled_toast::kBrightness, oled_toast::kLightbar, oled_toast::kSlot,
                            oled_toast::kPair, oled_toast::kFlip, oled_toast::kFlashSmall,
                            oled_toast::kPairedInSlot[0], oled_toast::kPairedInSlot[1],
                            oled_toast::kPairedInSlot[2], oled_toast::kPairedInSlot[3]}) {
        const char *zh = home_zh::notice(msg);
        CHECK(std::strcmp(zh, msg) != 0);
        Canvas c;
        c.zh_small_text(7, 52 + home_zh::font_offset(12), zh, 7, 52, 128, 64);
        check_canvas(c);
        c.invert(6, 52, 122, 12);
        if (msg == oled_toast::kPair) c.dump("home-pair-save-error");
    }
    Canvas c;
    home_zh::line(c, 12, ui_zh::kProjectName, 12);
    check_canvas(c);
}

TEST(zh_ds4_disabled_settings_and_notices_fit_both_layouts) {
    const Config_body saved = body();
    for (int group = 0; group < kNumGroups; ++group) {
        Nav nav; nav.in_group = true; nav.group = group;
        for (int row = 0; row < nav.rows(); ++row) {
            nav.row = row;
            Canvas c;
            draw(c, nav, View{saved, POWER_FLAGS_DEFAULT, saved, POWER_FLAGS_DEFAULT, false, nullptr, true});
            check_canvas(c);
            if (!ds4_item_supported(nav.item())) CHECK(c.contains("--"));
        }
    }
    check_compact_line("音频: 关");
    check_compact_line("扳机效果: 关");
    check_compact_line("DS4");
}

TEST(zh_ds4_pairing_diagnostics_fit_the_status_footer) {
    const char *messages[] = {
        "Pair: scan", "Pair: DS4 probe", "Pair: DS4 found", "Pair: DS5 found",
        "Pair: slot occupied", "Pair: ACL wait", "Pair: AUTH wait", "Pair: HID wait",
        "Pair: HID input", "Pair: FEAT wait", "Pair: FW wait", "Pair: CAL wait",
        "ACL command E:FF", "ACL failed E:FF", "AUTH failed E:FF",
        "Disconnected E:FF", "Feature CRC E:A3", "HID failed E:FF",
        "ACL timeout E:FF", "HID timeout E:FF", "FEAT timeout E:FF", "FA3 len:65535"
    };
    for (const char *message : messages) check_compact_line(message);
}
