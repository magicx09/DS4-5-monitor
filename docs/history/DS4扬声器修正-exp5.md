> 历史实验记录：其中的版本、测试结论、文件路径均对应当时状态；当前使用方法见 [项目首页](../../README.md)。

# DS4 扬声器修正（exp5）

exp4 已能让 Windows 枚举出扬声器，但实机无声。检查后发现有两个独立问题：音频 core1 在手柄连接前启动，旧代码因此没有初始化 DS4 SBC 编码器；同时 DS4 连接时还需要先写入扬声器功放增益。exp5 已修复这两点，并修正主机音量到 DS4 音量字段的映射。麦克风仍完全关闭。

[下载 prop16 UF2](firmware/ds4-experimental/ds5-ds4-speaker-exp5-zh-prop16.uf2)

SHA-256 会随构建写入 [SHA256SUMS.txt](firmware/ds4-experimental/SHA256SUMS.txt)。刷写后让 DS4 重新连接，再播放系统测试音。若 Windows 音量被调到静音，先取消静音并把播放音量调到 50% 以上。

本次 prop16 的主机测试、Flash 保留区、core1 RAM 路径和 XIP 检查全部通过；扬声器声音仍需以你的实机播放结果为最终判定。
