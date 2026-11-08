// Chinese strings and row formatting for every OLED screen. Pure, host-tested:
// oled.cpp draws these with the Fusion Pixel 12px Proportional face.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "diag_rows.h"

namespace ui_zh {

constexpr char kProjectName[] = "DS4/5-monitor";

constexpr int kX = 6;
constexpr int kLineH = 12;
constexpr int kMaxPx = 114;  // content from x=6 to the scroll markers at x=120

constexpr char kNoController[] = "无手柄连接";

// Slots
constexpr char kSlotsTitle[] = "配对槽";
constexpr char kConnected[] = "已连";
constexpr char kDisconnected[] = "未连";
constexpr char kEmpty[] = "空";
constexpr char kSlotHelp[] = "△切换 □长按清除";
constexpr char kSlotConnect[] = "按 PS 连接手柄";
constexpr char kSlotFull[] = "槽位已满先清除";
constexpr char kSwitched[] = "已切换";
constexpr char kSwitchFail[] = "切换失败";
constexpr char kWiped[] = "已清除";
constexpr char kWipeFail[] = "清除失败";

inline void slots_header(char *out, size_t n, int slot, bool connected) {
    std::snprintf(out, n, "配对槽 槽%d %s", slot, connected ? kConnected : kDisconnected);
}

inline const char *slots_footer(const char *status, bool connected, bool all_full) {
    if (status) return status;
    if (connected) return kSlotHelp;
    return all_full ? kSlotFull : kSlotConnect;
}

// Lightbar
constexpr char kLightbarTitle[] = "灯条";
constexpr char kLightbarSaveHint[] = "存:△0 ○1 ✖2 □3";
constexpr char kLightbarFooter[] = "R1切换模式";

inline const char *lightbar_tag(int mode) {
    switch (mode) {
        case 0: return "实时";
        case 1: return "预设0";
        case 2: return "预设1";
        case 3: return "预设2";
        case 4: return "预设3";
        case 5: return "呼吸灯";
        case 6: return "彩虹";
        case 7: return "渐变";
        case 8: return "主机";
        default: return "未知";
    }
}

inline const char *lightbar_hint(int mode) {
    switch (mode) {
        case 0: return "R1体感RGB";
        case 5: return "R1呼吸预设0";
        case 6: return "R1彩虹扫描";
        case 7: return "R1预设渐变";
        case 8: return "R1主机控制";
        default: return "R1锁定预设";
    }
}

// Trigger Test
constexpr char kTriggersTitle[] = "扳机测试";
constexpr char kTriggerFooter[] = "△切换模式";
inline const char *trigger_name(int preset) {
    static const char *const names[] = {"关", "反馈", "武器", "振动", "弓", "疾驰", "机械"};
    return preset >= 0 && preset < 7 ? names[preset] : names[0];
}

inline void trigger_mode(char *out, size_t n, int preset) {
    std::snprintf(out, n, "模式:%s", trigger_name(preset));
}

// Gyro / touchpad
constexpr char kGyroTitle[] = "体感倾斜";
constexpr char kTouchpadTitle[] = "触摸板";

inline void touchpad_fingers(char *out, size_t n, int fingers) {
    std::snprintf(out, n, "触摸点数:%d", fingers);
}

// Diagnostics
constexpr char kDiagTitle[] = "诊断信息";

inline void diag_uptime(char *line, size_t n, uint64_t us) {
    const uint64_t s = us / 1000000u;
    const uint64_t h = s / 3600u;
    std::snprintf(line, n, "运行 %lu:%02lu:%02lu",
                  static_cast<unsigned long>(h > 9999999u ? 9999999u : h),
                  static_cast<unsigned long>((s / 60u) % 60u),
                  static_cast<unsigned long>(s % 60u));
}

inline const char *diag_bt_state(bool connected) {
    return connected ? "蓝牙:已连接" : "蓝牙:等待中";
}

inline void diag_host02(char *line, size_t n, uint32_t v) {
    std::snprintf(line, n, "主机02:%lu", static_cast<unsigned long>(v));
}

inline void diag_trig_tx(char *line, size_t n, uint32_t trig, uint32_t tx) {
    std::snprintf(line, n, "扳机%s 发送%s", diag_rows::compact<4>(trig).s,
                  diag_rows::compact<4>(tx).s);
}

inline void diag_drops(char *line, size_t n, uint32_t raw, uint32_t enc, uint32_t hap) {
    std::snprintf(line, n, "丢包%s/%s/%s", diag_rows::compact<3>(raw).s,
                  diag_rows::compact<3>(enc).s, diag_rows::compact<3>(hap).s);
}

inline void diag_rate(char *line, size_t n, const char *label, uint32_t per_s) {
    std::snprintf(line, n, "%s%s/s", label, diag_rows::compact<6>(per_s).s);
}

inline void diag_mic_dec(char *line, size_t n, int32_t decoded, uint32_t wrote) {
    char dec[8];
    std::snprintf(dec, sizeof(dec), "%ld", static_cast<long>(decoded));
    std::snprintf(line, n, "麦解%s 写%s", dec, diag_rows::compact<4>(wrote).s);
}

inline void diag_mic_plc(char *line, size_t n, uint32_t v) {
    std::snprintf(line, n, "麦克PLC:%lu", static_cast<unsigned long>(v));
}

inline void diag_c1(char *line, size_t n, uint32_t loops, uint32_t enc) {
    std::snprintf(line, n, "核1 %s 编%s", diag_rows::compact<6>(loops >> 10).s,
                  diag_rows::compact<6>(enc).s);
}

inline void diag_pushes(char *line, size_t n, uint32_t raw, uint32_t hap) {
    std::snprintf(line, n, "原%s 触%s", diag_rows::compact<6>(raw).s,
                  diag_rows::compact<6>(hap).s);
}

inline void diag_bt_drop(char *line, size_t n, uint32_t v) {
    std::snprintf(line, n, "蓝牙丢包:%lu", static_cast<unsigned long>(v));
}

inline void diag_mic_lost(char *line, size_t n, uint32_t in, uint32_t buf, uint32_t under) {
    std::snprintf(line, n, "麦 i%s b%s u%s", diag_rows::compact<3>(in).s,
                  diag_rows::compact<3>(buf).s, diag_rows::compact<4>(under).s);
}

inline void diag_usb_ovr(char *line, size_t n, uint32_t v) {
    std::snprintf(line, n, "USB溢出:%lu", static_cast<unsigned long>(v));
}

// CPU / clock
constexpr char kCpuTitle[] = "CPU时钟";

inline void cpu_set(char *line, size_t n, uint32_t khz) {
    std::snprintf(line, n, "设定频率:%lu MHz", static_cast<unsigned long>(khz / 1000u));
}
inline void cpu_real(char *line, size_t n, uint32_t khz) {
    std::snprintf(line, n, "实测频率:%lu.%01lu MHz", static_cast<unsigned long>(khz / 1000u),
                  static_cast<unsigned long>((khz % 1000u) / 100u));
}
inline void cpu_vcore(char *line, size_t n, unsigned mv) {
    std::snprintf(line, n, "核心电压:%u.%02u V", mv / 1000u, (mv % 1000u) / 10u);
}
inline void cpu_vcode(char *line, size_t n, int vcode) {
    std::snprintf(line, n, "电压码:%d", vcode);
}
inline void cpu_temp(char *line, size_t n, int t10) {
    std::snprintf(line, n, "温度:%d.%d C", t10 / 10, (t10 < 0 ? -t10 : t10) % 10);
}

// BT signal
constexpr char kRssiTitle[] = "蓝牙信号";
constexpr char kInRange[] = "范围内";
constexpr char kUnknown[] = "未知";

inline const char *rssi_label(int8_t db) {
    if (db >= -5) return "优秀";
    if (db >= -15) return "良好";
    if (db >= -25) return "一般";
    return "较弱";
}
inline void rssi_value(char *out, size_t n, bool valid, int8_t db) {
    if (valid) std::snprintf(out, n, "信号:%d dB", static_cast<int>(db));
    else std::snprintf(out, n, "信号:--");
}
inline void rssi_link(char *out, size_t n, bool valid, int8_t db) {
    std::snprintf(out, n, "链路:%s", valid ? rssi_label(db) : kUnknown);
}

// Audio meters
constexpr char kVuTitle[] = "音频电平";
constexpr char kVuFooter[] = "USB音频峰值";
inline void vu_speaker(char *out, size_t n, uint8_t v) {
    std::snprintf(out, n, "扬声器 %3u", static_cast<unsigned>(v));
}
inline void vu_haptic(char *out, size_t n, uint8_t v) {
    std::snprintf(out, n, "触觉反馈 %3u", static_cast<unsigned>(v));
}

// Identify / boot fault
constexpr char kIdentifyTitle[] = "识别手柄";
inline void identify_slot(char *out, size_t n, uint8_t slot) {
    std::snprintf(out, n, "槽位%d", slot % 10);
}
constexpr char kFault1[] = "未找到无线芯片";
constexpr char kFault2[] = "此固件仅适用于";
constexpr char kFault3[] = "Pico 2 W";
constexpr char kBootBoard[] = "Pico 2 W + OLED屏";

}  // namespace ui_zh
