> 历史实验记录：其中的版本、测试结论、文件路径均对应当时状态；当前使用方法见 [项目首页](../../README.md)。

> 当前请使用 exp3：[0xA3 特征报告 CRC 修正](DS4特征报告修正-exp3.md)。exp2 存在连接后误校验断开问题。

# DS4 配对修正 exp2

实机反馈：exp1 中 DS4 持续快闪，OLED 一直等待配对。尚无该手柄的原始蓝牙抓包，不能确定所有失败原因。

已确认的源码遗漏：exp1 要求 GAP_EVENT_INQUIRY_RESULT 带完整设备名称。Pico SDK 的 BTstack 将普通／RSSI 扫描结果转换成此事件时，name_available 为 0；首版会跳过这种 DS4 候选。参考 DS4Dongle 则依据设备类别发现，不要求名称。

本版修正：
- 保留 DS5/Edge 原名称识别。
- 支持 EIR 中 Sony DS4 v1/v2 的 USB VID/PID（054C:05C4 / 054C:09CC）。
- 无名称且类别为游戏手柄时，进入 DS4 特征探测；明确的其他名称、键盘、鼠标等继续拒绝。
- 槽位归属规则仍生效；新 DS4 必须返回通过 CRC／长度检查的校准与固件报告，才保存槽位并枚举 USB。
- 在特征确认前保留配对窗口，探测失败后仍能按窗口策略继续扫描。
- OLED 状态页底部持续显示配对阶段或最后错误码；其他页面不覆盖，以便操作配对槽。

## 试用

使用与原布局对应的 `ds5-ds4-basic-exp2-zh-prop14.uf2` 或 `...prop16.uf2`，两份均为标准 Pico 2 W 实验镜像。原 exp1 和 home11 镜像保留。

选择空槽，回到状态页，长按板上 KEY0 两秒打开配对窗口；DS4 关机状态按 SHARE + PS 至快闪。观察底部诊断：

| 显示 | 含义 |
| --- | --- |
| Pair: scan | 扫描中／尚未接受候选；同时确认配对倒计时仍在运行 |
| Pair: slot occupied | 当前槽已属于另一手柄，换空槽 |
| Paired in slot N | 该手柄已经属于 N 槽 |
| Pair: DS4 probe | 发现无名称游戏手柄，准备验证 DS4 协议；通常很快进入下一阶段 |
| Pair: ACL wait | 正在建立蓝牙连接 |
| Pair: AUTH wait | 已建立连接，等待认证 |
| Pair: HID wait / HID input | 等待 HID 通道 |
| Pair: FEAT wait / FW wait / CAL wait | 等待手柄特征报告 |
| xxx E:xx | 前半部分表示失败阶段，后两位为十六进制 HCI 状态；timeout 的 FF 是本机超时标记，Feature CRC 的编号是报告 ID |

如果仍失败，记录最后停留的完整一行及 DS4 背面 CUH-ZCT1 / CUH-ZCT2 型号，以继续定位。底部短时间状态跳过并不表示没有执行。

## 验证

397 项主机测试通过（352 默认 + 22 prop14 + 22 prop16 + 1 真实 USB 生命周期），启用 ASan/UBSan。新增覆盖无名称扫描、EIR 型号、显式其他设备拒绝和诊断文字布局。

prop14/prop16 两份 exp2 构建成功，保存区、RAM 驻留、XIP 检查均通过。本次未刷写设备；修正了可复现的源码逻辑遗漏，但配对是否在用户手柄上恢复，仍待实机确认。
