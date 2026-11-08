# 更新记录

## 0.01 — 2026-11-08

DS4/5-monitor 首个公开版本，整合开发期 home11 和 exp1…exp11。

- Pico 2 W + Waveshare SH1107 中文 OLED，发布布局固定为 prop16。
- 接入真实 DS4 输入、触摸板、体感、普通震动、灯条及 SBC 扬声器播放。
- 修复配对识别与 DS4 0xA3 Feature 校验，接通 DS4 扬声器音量／目标选择与 OLED 菜单。
- 保留 DS5 原生 USB 音频身份与音频控制状态，音量锁启用／重连时自动恢复功放值，队列满时重试。
- 四个配对槽独立保存手柄设置；左右握把振动支持“关”，状态更新也遵守倍率。
- 主界面以实际 RSSI 信号条替代已连接状态右侧版本／型号文本。
- 启动画面与固件元数据改为 DS4/5-monitor，项目版本为 0.01。
- 整理硬件引脚、构建、使用、技术讨论、验证和来源许可；加入维护者提供的透明题图。

验证详情见 [docs/VALIDATION.md](docs/VALIDATION.md)。0.01 整包硬件回归尚待完成。原上游历史见 [CHANGELOG.upstream.md](CHANGELOG.upstream.md)。
