> 历史实验记录：其中的版本、测试结论、文件路径均对应当时状态；当前使用方法见 [项目首页](../../README.md)。

# DS4 内置扬声器：网络与当前代码核查

核查日期：2026-11-08。此次只检索资料、核对源码；没有修改固件、编译新 UF2 或刷写硬件。

## 结论与边界

DS4 内置扬声器存在从电脑驱动的公开实现，不能据“USB 直连只有耳机有声”判定硬件不支持。Windows 的播放设备名称、USB 音频接口是否枚举、手柄选择哪个物理输出，以及功放音量是否更新，是不同层次。

当时的实测表明：这只手柄的 USB 音频通路至少能驱动耳机。尚不能证明内置扬声器完好，也不能证明当前 Pico 固件的 USB PCM、SBC 编码和蓝牙发送已正常。没有做本机音频抓包，不能断言直连静音的具体控制寄存器原因。

## 公开来源

1. SensePost 的 [dual-pod-shock](https://github.com/sensepost/dual-pod-shock) 是独立的 DS4 扬声器播放示例。此次读取固定提交 `2200f5d593ccb6f66bed851c1bd5030ab257d0de`（2020-11-23）。[源码](https://github.com/sensepost/dual-pod-shock/blob/2200f5d593ccb6f66bed851c1bd5030ab257d0de/dual_pod_shock_POC.c) 同时包含内置扬声器音量字段和目标选择：`0x02` Speaker，`0x24` Headset。它使用包含状态与音频的 `0x15` 报告，与我们独立的 `0x17` 音频包不同；只把它作为物理能力和路由区分的证据，不能逐字节照搬。
2. [snipem/DS4Dongle README](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/README.md) 声明支持内置扬声器和耳机，并说明 `audio_follow_jack` 只控制何时暴露 USB 音频设备。`speaker_select=1` 强制内置扬声器；不能把默认跟随耳机插入的行为误认为内置扬声器不支持。
3. [DS4Dongle audio.cpp](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/audio.cpp) 实际按目标 `0x02/0x24` 构造 `0x17` SBC 报告；[bt.cpp](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/bt.cpp) 的 `ds4_set_volume()` 保留 `0xB0` 音量更新位。
4. [DS4Dongle USB 描述符](https://github.com/snipem/DS4Dongle/blob/2b1e3078b8059c3be422052466cc03a71844edb5/src/usb_descriptors.cpp) 的输出 Terminal 类型为 `0x0402` Headset，并标注与真实 DS4 v2 一致。这解释了为什么 Windows 有播放设备，不代表内置小扬声器自动接管系统音频；该资料是桥接项目的原机复刻描述，并非本次实测抓取的描述符。
5. [nefarius/DS4AudioStreamer](https://github.com/nefarius/DS4AudioStreamer) 提供电脑音频到 DS4 的蓝牙播放 POC，进一步支持“专用软件可以发送 DS4 音频”的判断。项目自己说明断线、切换设备处理不够可靠，不应视为成熟驱动。
6. [Reddit 对应讨论](https://www.reddit.com/r/DS4Windows/comments/xea5w4/how_do_i_play_audio_to_the_ps4_controllers/) 同时有“不支持”和游戏启动后成功发声的用户报告。只能当线索，不用社区口述替代协议与源码证据。
7. 从讨论追到 [DS4-Sound-Link](https://github.com/abh1shekbana/DS4-Sound-Link)。它宣称可切换内置扬声器/耳机，但 README 存在合并冲突残留，硬件验证文档仍有 Pending 项；音量范围等描述与 DS4Dongle 测量记录不一致。因此不依据它单方面的 `0x85` 握手、音量上限或帧长说明修改当前固件。

GitHub API 搜索遭遇匿名速率限制；改读公开 raw 文件、Git 仓库与网页。psdevwiki 本次返回 HTTP 403，未将它列为此次直接阅读的证据。

## exp5 已确定的问题

### 音量更新标志被过滤

`src/bt.cpp:1169` 在连接时设置 `p[0] = 0xB0`，`src/usb.cpp:60,103` 也在静音/音量更新时设置该值。所有调用经过 `bt_ds4_output()` → `ds4::output()`。

`src/ds4_protocol.h:108` 仍保留基础控制阶段的限制：

```cpp
bt[3] &= 7; // rumble, lightbar, blink only: no audio/microphone activation
```

因此 `0xB0 & 0x07 == 0x00`。音量数值虽然拷入报告，对应更新许可却丢失。exp5 的连接初始化与 USB 音量修正没有真正突破此过滤层。该缺陷由完整调用链确定，但仍不能据此保证修好它之后全部链路必然有声。

后续修复应给固件自有的扬声器/耳机音量控制独立构包入口，保留 `0x10/0x20/0x80` 更新位，同时保持麦克风 `0x40` 关闭。不要直接放开所有来自主机的状态位。

### OLED 音频菜单沿用旧能力屏蔽

`src/settings_menu.h:329` 的 `ds4_item_supported()` 排除了整个音频组（索引 6–15）。这由本地软件决定，与手柄是否报告音频能力无关。

当前 `ds4_audio.cpp` 硬编码目标 `0x02`，没有使用 `speaker_select`；PCM 增益使用 `volume[0]`，没有使用 OLED 的 `speaker_volume`。因此仅把菜单解锁也不会让设置真实生效。应连同后端路由和本地音量一起接通；DS5 专用增益/包模式以及未接入的麦克风项目继续不可调。

### 音量重复衰减隐患

`src/ds4_audio.cpp:135` 把 Windows 音量作为真实 dB 乘进 PCM；`usb.cpp` 又把相同音量映射为功放值。修复标志后，这会变成两次衰减。DS4Dongle 的实现把 Windows 音量交给功放映射，PCM 路径没有重复应用同一个值。应区分 Windows 功放控制和 OLED 本地数字增益。

## 下一次验证应回答的问题

- 主机 PCM 是否到达（读取帧数及非零峰值）；
- SBC 是否编码（成功/错误数）；
- `0x17` 是否进入发送队列、L2CAP 是否发送成功（这仍不等于手柄已播放）；
- 音量控制实际发出的允许位是否保留；
- 内置扬声器与耳机路由分开验证。

在证据尚未建立前，不换 SBC 参数，不猜测新增握手字节，不以“枚举成功”宣称音频成功。后续固件仍只构建 prop16。
