> 后续状态（2026-11-07）：DS4 基础控制现已作为 ENABLE_DS4 实验开关接入；下文“暂不加入”是 home11 原交付状态。当前范围、验证和限制见外层 `DS4基础支持-实验版说明.md`。

# 中文 OLED home11（2026-11-05）

本批沿用 Fusion Pixel 12px Proportional，并按国内 PS5／手柄玩家习惯统一术语：`已充满→充电完成`、`陀螺仪倾斜→体感倾斜`、`RT→实时`、`double/single→双包/单包`、`Haptics→触觉反馈` 等。设置页保留 14px 行框，其余紧凑页面使用 12px 行高。180° 软件翻转和 KEY0 + KEY1 长按交互保持不变。

## 汉化范围

- 主界面：连接、重新连接、配对倒计时、充电、充满、电量异常、配对和保存失败提示。
- 设置菜单：6 个分组、29 个条目、全部取值、页脚、状态提示和长按操作提示。
- 副页面：诊断信息、CPU 时钟、蓝牙信号、扳机测试、陀螺仪倾斜、触摸板、灯条、音频电平、配对槽。
- 识别与启动：就是这台／槽位、DS5 桥接器启动页、无线芯片缺失故障页。
- 通用提示：亮度、灯条、槽位、配对、闪存空间不足等提示条。

版本号、蓝牙地址、协议计数、MAC、L2／R2／R1、PS／KEY0、X／Y／Z 轴、DS5／DSE 型号、Pico 2 W 板名，以及 MHz、V、C、dB、Hz 等单位按要求保留为拉丁字符。

## 本批修复

- 屏幕 180° 翻转在 `oled_flush::rotate_180` 中交换行顺序、字节顺序并反转位图，再送入现有的 A0/C0 刷新路径；home8 的 A1/C8 硬件重映射已移除。切换后强制整帧重发，避免局部刷新残留。方向保存在配置区新字段中，旧配置的 0xFF 按正常方向读取。
- 长按 KEY0 + KEY1 2 秒只触发一次翻转，释放后不会切页或改变亮度；继续按住到 5 秒执行重启，兼容原有关机重启入口。
- 从 Fusion Pixel 12px Proportional 官方 2026.10.25 BDF 生成单套 327 字形子集，覆盖所有页面中文、ASCII 和按键符号。Fusion Pixel 缺 U+2716“✖”，生成器用同字体 U+00D7“×”的位图补位。
- 术语按玩家习惯收敛：主界面用“手柄状态”“充电完成”“按 PS 连接”；设置页用“左/右握把振动”“扳机效果强度”“自动触觉反馈”“扬声器音量/增益”“麦克风来源/电平”“回报率”“实时”；灯条用“实时/预设/呼吸灯”；副页面用“体感倾斜”“触摸点数”“识别手柄”。
- 诊断页仍保留少量紧凑缩写（如 `核1`、`麦解`、`原/触`），以适配 128×64 的宽度；对应的完整术语在交付包 `DS5-OLED-英文-中文对照表.md` 中说明。
- 设置页标题、条目、取值和底栏全部使用 12px 比例字库；14px 行框显示两行内容加底栏，16px 行框显示三行内容。普通副页面按 12px 行高重排，诊断页改为 4 行滚动视图，配对槽保留 4 行并省略底栏。
- 主界面状态文字、提示条、启动页、识别页和故障页全部改为 12px 比例字库；主界面继续使用 12px 行框。
- 配对指引保持“长按分享键+PS”，不使用“创建键”。
- 长按 △ 与页面、条目绑定。按住时移动条目、进入分组或返回，会取消该次操作；必须松开后重新按住。避免把已累计的长按时间带到恢复默认或清空配对，也避免返回时误变成一次保存。按键修复同时用于英文和中文。
- 16px 重置页保留长按提示；普通页面出现状态提示时临时显示两行列表，调整滚动位置和滚动条，保留选中项；提示结束后恢复三行。
- 恢复中文选中行中“未保存值”的暗色窗口与亮色尾帽。数值右边缘向左移动 2px，为标识留出空间。
- UTF-8 解码验证合法标量，支持四字节编码，拒绝过长编码、代理区和超过 U+10FFFF 的值；截断时不读取 NUL 后的数据。非法字节逐字节显示 `?`，未收录的合法字符显示一个 `?`，测宽和绘制一致。
- 点阵绘制先计算字形与裁剪框的交集，再遍历有效区域，减少逐像素的边界判断。不宣称已测得实机性能提升。
- XIP 检查脚本使用 POSIX awk 可识别的十进制常量，消除 macOS 的误报。实际 ELF 的分频 2、电压码 11 通过；错误期望 3、13 分别拒绝。
- 运行时不再引用 Ark 或 Fusion Pixel 10px 字库。字库生成脚本只读取本地 BDF，不联网下载。
- Fusion Pixel 12px Proportional 采用 SIL Open Font License 1.1，许可文本随源码放在 `src/FUSION_PIXEL_OFL.txt` 与 `src/FUSION_PIXEL_LICENSES/`。

## 自动验证

```sh
tests/host/run.sh ../build-host
```

脚本通过 CTest 执行默认测试和两种中文组合（prop14／prop16），均启用 ASan/UBSan。默认目标 338 项，每种中文目标 20 项（15 项中文渲染／字体／全页面文案测试加 5 项按键回归），合计 378 次测试执行，失败 0。

中文测试直接调用 `settings_menu::draw`、`home_zh`、`ui_zh`、`oled_zh::draw` 与 `oled_zh_home::draw`。覆盖全部设置条目与取值、主界面连接状态、最多 599 秒的配对倒计时、充电 ETA、提示条、启动标题、识别页文案，以及所有副页面的固定文案和最长格式化结果；检查字形存在、宽度不超过 114px、行框内无裁切。截图来自真实 C++ 逻辑帧缓冲，不是实物 OLED 照片。可通过 `OLED_ZH_DUMP=/已存在目录` 导出 PGM。

按键测试覆盖 2 秒前不翻转、2 秒触发一次、按住到 5 秒才重启、释放后不误导航，以及重启/翻转组合不会带入亮度或翻页。旋转测试逐像素检查 180° 映射和点数量不变。配置测试覆盖翻转状态独立保存、重启恢复、旧页面默认正常方向和写入失败。

## 固件构建

需要 CMake、Ninja、Python、ARM GNU Toolchain 15.2.Rel1，设置 `PICO_SDK_PATH` 与 `PICO_TOOLCHAIN_PATH`。SDK 固定为 2.3.1 / `079c6f39023649b154152db30f1d781e884879bc`，其 TinyUSB 固定为 `98741b55959564746998d0453c32d87064629d7c`。WDL / Opus 源码沿用迁移快照；Git 子模块引用保持原样。

```sh
cmake -S . -B ../build-prop14 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DPICO_BOARD=pico2_w -DENABLE_COMPANION_VENDOR=OFF \
  -DOLED_ZH=ON -DOLED_SETTINGS_ROW_HEIGHT=14 \
  -DVERSION=v0.9.5-oled-edition-home11
cmake --build ../build-prop14 --target ds5-bridge -j 4
python3 tools/check_flash_reserved.py \
  ../build-prop14/ds5-bridge-oled.elf ../build-prop14/ds5-bridge-oled.uf2
bash tools/check_ram_residency.sh ../build-prop14/ds5-bridge-oled.elf
bash tools/check_xip_setup.sh ../build-prop14/ds5-bridge-oled.elf 2 11
```

第二个组合使用独立目录 prop16=16。CMake 自动添加 `+zh-prop行框` 后缀；mono 构建已移除。

本批沿用默认 USB 配置（`ENABLE_COMPANION_VENDOR=OFF`）。两份构建均通过上述三项 ELF/UF2 检查：镜像按扇区取整截至 `0x10155000`，保存区从 `0x103f9000` 开始，余量 2704 KiB。Core 1 音频与 Core 0 蓝牙／USB 热路径仍在 RAM；嵌入式 XIP 初始化与电压设置正确。没有更改 USB 描述符、配对协议或音频处理算法；配置 body 和 CRC 布局未变，只在 body 外新增翻转字节。新增的 8 KiB 发送缓冲位于 RAM，RAM 驻留检查通过。

## 实机验收尚待完成

需要实机检查：全部页面 Fusion Pixel 12px Proportional 的清晰度与反白、设置页 14／16px 行框、配对槽四行布局、诊断四行滚动、KEY0 + KEY1 长按 2 秒翻转、5 秒重启、重启后方向恢复、松手不误触、配对倒计时、充电 ETA、最长数值、保存与掉电恢复、蓝牙重连、USB 识别、音频、振动与自适应扳机。本批未烧录硬件，不宣称实机通过。DS4 仍暂不加入。
