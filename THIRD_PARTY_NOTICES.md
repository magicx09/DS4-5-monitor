# 许可与第三方声明

## 应用代码

DS4/5-monitor 0.01 整合应用按根目录 [GNU AGPL v3](LICENSE) 发布。本次新增修改同样按 AGPL v3 提供。

理由是继承源码 `src/remap.cpp` / `src/remap.h` 明确注明从 SundayMoments/DS5_Bridge 移植，而对照提交 `ba839b5104ffb775a4f20a3f5426be98b387c1a8` 的许可为 AGPL v3。`src/audio.cpp` 也标注了该项目的设计来源。整合应用因此采用 AGPL v3，并保留各文件的原始归属。

原 DS5Dongle / OLED Edition 的 MIT 文本保存在 [LICENSES/DS5Dongle-MIT.txt](LICENSES/DS5Dongle-MIT.txt)，原有版权、文件内注释和其他声明继续有效。DS4Dongle 的声明保存在 [src/DS4Dongle-LICENSE.txt](src/DS4Dongle-LICENSE.txt)。AGPL 文本副本：[LICENSES/DS5_Bridge-AGPL-3.0.txt](LICENSES/DS5_Bridge-AGPL-3.0.txt)。上游原作的授权保持不变。

源码、构建脚本和固定子模块提交随仓库提供；固件 Release 对应源码 tag。分发修改版本时，应保留这些声明并履行相关许可证的源码提供等要求。

## 依赖许可

- Pico SDK：保留 `LICENSES/Pico-SDK.txt` 及源码自身的第三方声明。
- TinyUSB：保留 `LICENSES/TinyUSB.txt`。
- BTstack：保留 `LICENSES/BTstack.txt`。其标准许可包含非商业条件，具体平台／商业使用须查阅权利人的适用授权，不能从本项目代码许可推导出商业使用许可。
- CYW43：保留 `LICENSES/CYW43.txt`、`LICENSES/CYW43-Raspberry-Pi.txt` 与无线固件许可；Raspberry Pi 专用条款限定其适用硬件范围。
- BTstack 的 Bluedroid SBC：保留其第三方许可证副本及 SDK 子模块中的原始声明。
- Opus：保留 `LICENSES/Opus.txt` 和 `lib/opus/COPYING` 等附带文件。
- WDL 重采样：保留 `LICENSES/WDL-resample.txt`；子模块中各文件自己的许可继续有效。

这些组件按各自许可提供。应用代码许可不能覆盖无线栈、固件或芯片厂商的附加条款；在其他硬件、商业产品或重新分发场景中，需要单独核实这些组件的授权组合。

## 字体与图片

当前字体：Fusion Pixel 12px Proportional，作者 TakWolf，SIL Open Font License 1.1。保留 `src/FUSION_PIXEL_OFL.txt`、`src/FUSION_PIXEL_LICENSES/` 以及 `fonts/fusion-pixel-12px-proportional/OFL.txt`。历史 Ark 字体许可也保留。

`docs/images/mascot.png` 为维护者提供的题图，处理只涉及去背景和透明裁切。原作者／授权未额外声明，**不适用本仓库代码开源许可证**；第三方复用图片应向相应权利人确认。屏幕预览由项目点阵渲染／布局生成；旧上游截图保留原归属。

完整来源链、参考用途和固定提交见 [docs/REFERENCES.md](docs/REFERENCES.md)。
