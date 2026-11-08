# 来源、参考项目与致谢

以下记录代码来源、协议对照和构建依赖，并说明各项目在本分支中的用途。更新于 2026-11-08。

## 代码来路

| 来源 | 在本项目里的位置 | 归属说明 |
| --- | --- | --- |
| [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle) | 核心 Pico 蓝牙／USB 桥接、音频、手柄报告等 | 基础上游，MIT |
| [MarcelineVPQ/DS5Dongle-OLED-Edition](https://github.com/MarcelineVPQ/DS5Dongle-OLED-Edition) | OLED 页面、配对／设置、诊断、测试与构建检查等 | 直接基础仓库；恢复的 Git 基点 `08536e0731cc2b529de8e56b94c00146e193321c`，原版权保留 |
| home11 中文交付 | Fusion Pixel 中文界面、prop14/16、按键／旋转等交付改动 | 来自中文界面开发快照；部分早期提交对象缺失，Git 历史以可恢复的上游基点为准 |
| [snipem/DS4Dongle](https://github.com/snipem/DS4Dongle/tree/2b1e3078b8059c3be422052466cc03a71844edb5) | DS4 描述符、报告、Feature 校验及 SBC 音频／音量／目标选择参考 | 固定对照 `2b1e3078b8059c3be422052466cc03a71844edb5`，MIT；保留 `src/DS4Dongle-LICENSE.txt`；它是完整固件，不是可直接链接的库 |
| [SundayMoments/DS5_Bridge](https://github.com/SundayMoments/DS5_Bridge/tree/ba839b5104ffb775a4f20a3f5426be98b387c1a8) | 继承源码标注的 remap 应用逻辑／按钮集合、麦克风抖动缓冲设计；另用于对照 USB 串号和 Windows 身份文档 | 固定对照 `ba839b5104ffb775a4f20a3f5426be98b387c1a8`。该版本许可证为 **AGPL-3.0**，整合应用据此采用 AGPL-3.0，原 MIT 声明另行保留 |
| [zurce/DS5Dongle-OLED](https://github.com/zurce/DS5Dongle-OLED) | 上游引用的 OLED 图标思路、长按交互、四槽配对 | 归属记录继承自基础分支 |
| [loteran/DS5Dongle](https://github.com/loteran/DS5Dongle) | 上游归属的自动触觉 DSP、扬声器回归修正、UAC 与持久配置分离、Linux 配置脚本 | 作为继承来源列出 |

DS4/5-monitor 主要修改：Windows 本机开发迁移、DS4 后端接入与配对／Feature 修正、DS4 扬声器路径及 OLED 控制、DS5 原生双路音频状态修复、每槽独立设置、真正的振动关闭、主界面信号条和对应回归测试。公开版本从 **0.01** 开始，开发期 exp1…exp11 是实验编号，不与上游版本对应。

## 扬声器与原生音频的直接对照

- [sensepost/dual-pod-shock](https://github.com/sensepost/dual-pod-shock/blob/2200f5d593ccb6f66bed851c1bd5030ab257d0de/dual_pod_shock_POC.c)：DS4 内置扬声器／耳机的独立 POC，固定提交 `2200f5d593ccb6f66bed851c1bd5030ab257d0de`。用于证明播放路径和目标字段存在；其 0x15 打包不能直接替换本项目 0x17。
- [nefarius/DS4AudioStreamer](https://github.com/nefarius/DS4AudioStreamer)：PC 到 DS4 蓝牙音频 POC。作为能力与实现路径参考，没有把它打包为本项目运行依赖。
- [abh1shekbana/DS4-Sound-Link](https://github.com/abh1shekbana/DS4-Sound-Link)：社区实现；核查时文档存在冲突残留，握手／音量描述未用作本项目的实现依据。
- [DS4Dongle audio.cpp](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/audio.cpp)、[bt.cpp](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/bt.cpp)、[USB 描述符](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/usb_descriptors.cpp)：实际协议、音量允许位、音频目标及主机枚举的主要代码证据。
- [DS5_Bridge USB 描述符](https://github.com/SundayMoments/DS5_Bridge/blob/ba839b5104ffb775a4f20a3f5426be98b387c1a8/src/usb_descriptors.c)、[Windows 设备身份说明](https://github.com/SundayMoments/DS5_Bridge/blob/ba839b5104ffb775a4f20a3f5426be98b387c1a8/docs/windows-device-cleanup.md)、[awalol e79c762](https://github.com/awalol/DS5Dongle/commit/e79c762)：DS5 USB 串号对照。该差异不等于已经证明游戏如何识别设备。
- [Reddit 讨论线索](https://www.reddit.com/r/DS4Windows/comments/xea5w4/how_do_i_play_audio_to_the_ps4_controllers/)：仅作为检索线索。社区“不能播放”与“能播放”都不能替代具体协议和实测。

核查时 psdevwiki 返回 403，未能读取。排查记录见 [DS4 扬声器核查](history/DS4扬声器-网络与代码核查.md)和 [DS5 现场音量恢复](history/DS5原生双路音频-exp9.md)。这些日志保留各实验版本当时的结果，当前功能以首页为准。

## 基础分支保留的协议与生态参考

- [nowrep/dualsensectl](https://github.com/nowrep/dualsensectl)：自适应扳机测试效果的报告打包参考。
- [rafaelvaloto/Pico_W-Dualsense](https://github.com/rafaelvaloto/Pico_W-Dualsense)：上游项目灵感。
- [rafaelvaloto/Dualsense-Multiplatform](https://github.com/rafaelvaloto/Dualsense-Multiplatform)：继承文档中的跨平台 DualSense 资料。
- [egormanga/SAxense](https://github.com/egormanga/SAxense)：蓝牙触觉 POC。
- [Paliverse/DualSenseX](https://github.com/Paliverse/DualSenseX)：上游引用的扬声器报告资料。
- [Sony DualSense 报告资料](https://controllers.fandom.com/wiki/Sony_DualSense)、[Linux hid-playstation](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c)：报告结构／驱动行为对照。列为参考不等于导入了完整驱动。
- [SDL](https://github.com/libsdl-org/SDL)、[daidr/dualsense-tester](https://github.com/daidr/dualsense-tester)：继承代码中 Edge 附加按键布局的注释来源。
- [MarcelineVPQ/DS5Dongle-OLED-Config-Web](https://github.com/MarcelineVPQ/DS5Dongle-OLED-Config-Web)、[awalol/ds5dongle-config-web](https://github.com/awalol/ds5dongle-config-web)：原配置器及其上游；本仓库不包含配置器，本版关闭其所需的 USB 管理接口。
- [Zacksly / PS5 Button Icons and Controls](https://zacksly.itch.io/ps5-button-icons-and-controls)：上游配置器文档归属的图标（CC BY 3.0），不是本项目机器人题图的来源，也没有将该配置器打包进此仓库。

## 构建、运行依赖与字体

| 项目 | 用途／固定版本 |
| --- | --- |
| [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) | 2.3.1，`079c6f39023649b154152db30f1d781e884879bc` |
| [TinyUSB](https://github.com/hathach/tinyusb) | `98741b55959564746998d0453c32d87064629d7c`，USB HID / Audio |
| [BlueKitchen BTstack](https://github.com/bluekitchen/btstack) | SDK 固定子模块，蓝牙栈及 SBC 接口；包含第三方 Bluedroid SBC 实现 |
| [CYW43 driver](https://github.com/georgerobotics/cyw43-driver) | SDK 固定无线驱动与固件，保留平台相关许可 |
| [Xiph Opus](https://github.com/xiph/opus) | 子模块 `2d862ea14b233e5a3f3afaf74d96050691af3cd5`，DS5 音频 |
| [Cockos WDL](https://github.com/justinfrankel/WDL) | 子模块 `afc0f78dca4a1743a948839ac75f4d7059d739c3`，重采样 |
| [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font) | 12px Proportional，中文 OLED 当前字体；SIL OFL 1.1，保留派生字体许可目录 |
| [Ark Pixel Font](https://github.com/TakWolf/ark-pixel-font) | 历史保留字库／许可，不是本版当前运行字体 |
| ARM GNU Toolchain / [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) | 固定 Windows 交叉编译器和主机 ASan/UBSan 环境，见依赖锁文件 |
| CMake / Ninja / Python / Git | 构建、测试、字库生成；不需要 Docker 或 WSL 才能本机编译 |

## 开发快照与题图

中文界面开发快照包含部分未提交修改。公开仓库保留可恢复的上游历史，缺失的中间提交没有补造。Windows 适配记录见 [windows-port.patch](history/windows-port.patch)。

题图由维护者提供，已去除背景并裁切透明留白，角色原有颜色保持不变。图片不包含在代码许可证中，授权说明见 [第三方声明](../THIRD_PARTY_NOTICES.md)。
