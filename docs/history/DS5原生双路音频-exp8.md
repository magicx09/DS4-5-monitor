> 历史实验记录：其中的版本、测试结论、文件路径均对应当时状态；当前使用方法见 [项目首页](../../README.md)。

# DS5 原生双路音频兼容修正 exp8

> 后续实测：exp8 单独刷入未解决静音。随后在本机确认游戏同时向音箱和 DS5 输出音频；仅把 DS5 端点音量从 100% 调至 90% 再恢复 100%，原生手柄声音恢复。持续方案及证据见 `DS5原生双路音频-exp9.md`。不要把 exp8 的设备识别修正当作本次故障的已证实原因。

日期：2026-11-08。目标是电脑音箱播放主音轨、DS5 接收游戏原生手柄音效；不录制或复制系统混音，不按游戏名称分支。

## 本次交付

- `firmware/ds4-experimental/ds5-ds4-native-audio-exp8-zh-prop16.uf2`
- SHA-256：`930BA58F0D8B3B1109F402F491E31EA91D6096DD216EE5BCB602D9B100870471`
- 仅编译 prop16，保留 ENABLE_DS4 和已经实测有声的 exp7 DS4 音频路径。
- exp7 UF2 原件保持不变，SHA-256 仍为 `DF9E77D6AE92D231208DE734D1F365235ED7EEFE83DEAC170C5FDEA42787C5F5`。
- 没有刷写硬件、修改 Windows 默认音频设备、清理 PnP 缓存或提交 Git。

## 代码依据和改动

### 有线身份

OLED 分支的 USB 描述符重新启用了 iSerialNumber，并返回控制器 MAC 字符串。上游 awalol 的 e79c762 曾移除序列号；SundayMoments/DS5_Bridge 固定提交 ba839b5104ffb775a4f20a3f5426be98b387c1a8 的 usb_descriptors.c 亦明确使用 iSerialNumber=0，并说明 SpecialK 将 PlayStation HID 序列号作为蓝牙身份线索。

- [上游修正后的描述符](https://github.com/awalol/DS5Dongle/blob/e79c762/src/usb_descriptors.cpp)
- [DS5_Bridge 描述符](https://github.com/SundayMoments/DS5_Bridge/blob/ba839b5104ffb775a4f20a3f5426be98b387c1a8/src/usb_descriptors.c)
- [DS5_Bridge Windows 身份说明](https://github.com/SundayMoments/DS5_Bridge/blob/ba839b5104ffb775a4f20a3f5426be98b387c1a8/docs/windows-device-cleanup.md)

exp8 对 DS5/DSE 恢复无序列号身份，并拒绝其未声明的串号字符串请求。DS4 仍用既有 MAC 序列号，唤醒键盘仍用独立序列号。Sony VID、DS5/DSE PID、接口顺序、4 通道 48 kHz/16-bit PCM 和 HID 描述符不变。Feature 0x09 的配对信息不变。

证据边界：这是可确认的上游兼容性差异，不代表已经证明某个游戏使用与 SpecialK 相同的识别算法，也不能保证移除串号单独解决所有原生音频问题。无串号后，Windows 可能按 USB 端口生成新的设备及音量偏好，首次连接应检查音箱仍为默认输出。

### 游戏音频状态保留

原代码 main.cpp 将主机 0x02 按原允许位转发为蓝牙 0x31，已经包含音量/路由/静音字段；因此不能说旧固件完全丢弃了游戏控制。

但 state_mgr.cpp 的 state_update() 把 VolumeHeadphones、VolumeSpeaker、VolumeMic、AudioControl、MuteControl、AudioControl2 的缓存更新注释掉了。bt_send_state() 在连接阶段和 USB 枚举后的 mic_status_task() 中重发全状态时，会回放旧音量/默认路由，存在覆盖较早游戏初始化的时序问题。

exp8 按各自允许位缓存以上字段，音量和静音缓存也遵守已有 lock_volume。无关灯条/扳机报告不改变已保存的音频控制；全状态重发保留游戏路由和前置增益，同时继续应用 OLED 用户覆盖值。

USB Audio 端点音量/静音单独记录。连接断开时清除上一局游戏的 HID 音频覆盖，恢复 USB 端点状态，避免把上个手柄的静音或功放关闭状态带进下一次连接。USB 静音更新只改 mic/speaker/headphone 三个位，保留同字节中的游戏触觉静音及省电位。不增加麦克风功能或启用新的麦克风流。

## 自动验证

执行 `python dev.py ds4`：

- 原主机测试 355 项。
- prop14/prop16 中文渲染主机测试各 22 项（仅主机测试，不生成 prop14 固件）。
- DS4 USB 生命周期 1 项。
- 新增实际 state_mgr.cpp 音频状态测试 6 项：延后重发、无关/短报告、音量锁、USB 静音位、重连清理、用户增益覆盖。
- 新增实际 usb_descriptors.cpp 回调测试 2 项：DS5/DSE/DS4/键盘身份切换，原生四通道格式及 HID 接口长度。使用本机固定版本 TinyUSB 头文件。
- 共 408 项，ASan/UBSan 下通过。
- 闪存检查通过：镜像至 0x10159000；保存区从 0x103f9000，间隔 2688 KiB。
- RAM 热路径和 XIP/电压检查通过。

日志在 `logs/host-tests.log`、`logs/ds4-prop16-build.log` 和对应 verify 日志。

## 使用与验收

刷入 exp8 后重新插拔桥接器，连接 DS5。电脑音箱作为 Windows 默认输出，桥接器的扬声器端点保持启用；OLED 手柄类型用 DS5 或与实物一致的自动类型，音频输出为扬声器。游戏需要能够直接访问 DualSense；若 Steam Input/虚拟 Xbox 替换了设备，原生特性不一定可见。

验收目标：主音轨持续从电脑音箱播放，游戏专用武器/脚步等音效从 DS5 扬声器播放，同时原生触觉、扳机及输入保持正常。测试时不运行系统混音镜像软件。

当前主机只读检查未发现已连接的 Sony DS5/DSE/DS4 USB 设备，无法对新镜像做实机播放验收。自动测试证明代码和描述符行为，不能代替实机听音；交付状态是兼容修正版待刷机验证，不能标记为目标已实机完成。
