> 历史实验记录：其中的版本、测试结论、文件路径均对应当时状态；当前使用方法见 [项目首页](../../README.md)。

# DS4 连接后断开修正 exp3

2026-11-07。实机反馈：exp2 已能连接，几秒后断开，OLED 显示 `Feature CRC E:A3`。

## 已确认原因

移植代码把所有 DS4 特征报告都当作带 CRC 的数据，错误地要求 `0xA3` 固件信息报告通过 CRC。该报告实际没有 CRC。它被丢弃后，初始化始终等不到固件信息；连接看门狗达到超时便主动断开。

依据：[Linux hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c)，本次读取的 `dualshock4_get_firmware_info()` 明确写道：

> Note USB and BT support the same feature report, but this report lacks CRC support, so must be disabled in ps_get_report.

函数以 49 字节（含报告 ID）读取 0xA3，CRC 参数为 false；`dualshock4_get_calibration_data()` 对蓝牙 0x05 则以 41 字节、CRC 参数 true 读取。外层 HIDP 头额外占 1 字节，所以固件 L2CAP 回调看到的长度分别是 50、42。

此前移植参考源里“所有特征报告去掉末尾 CRC”的处理也不适用于 0xA3。本次不继续沿用该假设。

## 修复

- 0xA3：验证 HIDP 头、报告 ID 和准确长度，不做不存在的 CRC 校验。
- USB 转发完整保留 48 字节固件信息，不再删掉最后四字节后补零。
- 0x05 校准数据继续严格验证长度与 CRC。
- 0x11 蓝牙输入报告的 CRC 校验保持不变。
- 异常长度显示 `FA3 len:N` 等诊断，N 为含 HIDP 头的实收长度，方便识别实际协议差异。
- 输出使用 exp3 文件名，原 exp1、exp2 和 home11 镜像保留。

## 验证

主机测试 400 项通过：355 默认逻辑、22 prop14 中文、22 prop16 中文、1 真实 USB 生命周期。新增测试证明无 CRC 的 0xA3 被接受且尾部数据完整转发、损坏的校准包继续拒绝、截断和过长报告不越界。

固件和镜像检查结果以 `logs/ds4-prop14-*`、`logs/ds4-prop16-*` 为准。本次未刷写设备；真实手柄上的断开问题是否解除仍需验收。

## 使用

使用对应布局的 `ds5-ds4-basic-exp3-zh-prop14.uf2` 或 `...prop16.uf2`。不需要恢复默认或清空全部配对。保留上次选择的槽，先按 DS4 的 PS 尝试重连；若原初始化未完成保存，则重新 SHARE + PS 配对。

如果仍断开，请记录底部完整错误行；若已稳定连接，继续检查电脑是否出现手柄、按键、摇杆、触摸、体感、震动及灯条。
