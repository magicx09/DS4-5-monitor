# 硬件与安装

## 对应硬件

| 部件 | 要求 | 备注 |
| --- | --- | --- |
| 主板 | **树莓派 Pico 2 W 开发板** | RP2350，带无线功能；当前发布镜像只面向 Pico 2 W |
| 显示器 | **微雪 树莓派 Pico 1.3 寸 OLED 显示屏模块扩展板，SH1107 芯片** | 官方型号 Pico-OLED-1.3，带 KEY0 / KEY1；64×128 面板横向显示为 128×64 |
| USB 线 | 支持数据传输、接口与开发板匹配 | 原版 Pico 2 W 为 Micro-USB；仅充电线不能完成 USB 枚举 |
| 排针／排母 | 与两块板匹配，焊接可靠 | 没有预焊排针的主板需要先焊接；按板上丝印确认方向 |
| 手柄 | 真实 DS4 / DS5 | Edge 代码继承自上游，尚未完成本版实测；第三方仿制手柄不在已验证范围 |
| 主机 | 当前以 Windows PC 为验证对象 | Linux / macOS 的固件代码与工具可能可用，但没有本版完整验收记录 |

## 关于商品名里的“I2C 接口”

部分商品标题将这块 SH1107 扩展板标为“I2C 接口”。[微雪官方资料](https://www.waveshare.com/wiki/Pico-OLED-1.3)列出的接口是 **4-wire SPI / I²C**。

当前代码 `src/oled.cpp` 使用 `spi1`，时钟为 10 MHz，并驱动 D/C、CS、RESET。**0.01 发布镜像按 SPI 模式工作，没有实现这块屏的 I²C 发送后端。** 购买时请确认是对应扩展板，并按该板版本的官方说明确认接口选择焊盘／电阻配置。SH1107 型号相同不代表接线相同；已正常工作的模块无需改焊。

## 固件实际引脚

| 信号 | Pico GPIO | Pico 物理脚位 | 用途 |
| --- | --- | --- | --- |
| OLED D/C | GP8 | 11 | 命令／数据选择 |
| OLED CS | GP9 | 12 | 片选 |
| OLED CLK | GP10 | 14 | SPI1 SCK |
| OLED DIN / MOSI | GP11 | 15 | SPI1 发送数据 |
| OLED RST | GP12 | 16 | 屏幕复位 |
| KEY0 | GP15 | 20 | 板载按键，低电平有效 |
| KEY1 | GP17 | 22 | 板载按键，低电平有效 |
| 电源与地 | 按扩展板官方原理图 | 对插时由模块连接 | Pico GPIO 为 3.3 V 逻辑，不要把 5 V 接入 GPIO |

标准模块可直接对插。断电后安装，对齐 USB 端和板上丝印，检查是否错位一排、连锡或焊点虚接，再通电。单独飞线时请同时核对微雪模块原理图和 Pico 2 W 官方 pinout，以上表格不替代电源布线图。

## 不属于本版即插即用范围

- 无无线的 Pico / Pico 2，或把 Pico W 的镜像当成 Pico 2 W 镜像使用。
- SSD1306 等不同控制器、纯 I²C 四针模块、其他分辨率屏幕。
- 同时连接两只手柄；四个配对槽只记忆四只设备。

## 官方资料

- [Raspberry Pi Pico 2 W](https://www.raspberrypi.com/products/raspberry-pi-pico-2-w/)
- [Pico 系列官方文档与 pinout](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html)
- [Waveshare Pico-OLED-1.3 Wiki / 原理图 / SH1107 数据手册入口](https://www.waveshare.com/wiki/Pico-OLED-1.3)

接口说明已于 2026-11-08 对照官方页面及本地源码核查。
