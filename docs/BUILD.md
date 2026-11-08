# 构建与本机开发

公开发布目标固定为 **Pico 2 W / 150 MHz / ENABLE_DS4=ON / 中文 prop16**。主机渲染测试仍覆盖 prop14 和 prop16，但不生成 prop14 固件。

## 获取源码

```sh
git clone --recurse-submodules https://github.com/magicx09/DS4-5-monitor.git
cd DS4-5-monitor
```

不要只下载网页 ZIP 后直接编译：GitHub 源码 ZIP 不包含 Opus / WDL 子模块内容。版本在根目录 `VERSION`，当前为 `0.01`。内部目标名 `ds5-bridge` 和 ELF 中间文件名保留，以减少构建检查改动；发布资产和启动画面使用项目新名称。USB 的 Sony 兼容身份不随项目改名。

## Windows 原生环境

需要 Python 3.11+、Git for Windows、CMake、Ninja。CMake / Ninja 可以来自 Visual Studio Build Tools 的 C++ CMake 工具组件，也可安装后加入 PATH。ARM 与 LLVM-MinGW 不需要先全局安装。

```powershell
python tools/bootstrap_windows.py
python tools/dev_windows.py all
```

bootstrap 下载并校验 ARM / LLVM-MinGW，固定 Pico SDK 和 TinyUSB，初始化子模块，并编译安装本机 pioasm / picotool。其默认工作区是仓库内被忽略的 `.local/`；如已有独立工具目录，可在当前终端设置 `$env:DS45_DEV_HOME = 'D:/ds45-dev'`。脚本不改系统 PATH，不需要管理员，不负责刷机。已经存在但提交不匹配的独立 checkout 会报错，避免静默丢弃修改。

`all` 执行固定依赖检查、主机测试、prop16 编译与三项镜像检查。日志在 `.local/logs/`，产物在 `.local/firmware/ds4-experimental/`（目录名沿用实验期，最终文件使用新名称）。

其他命令：`doctor` 查看依赖；`test` 只跑测试；`build` 编译并检查；`frames` 导出两种主机渲染图；`font` 从随仓库提供的 BDF 重建字体。`ds4` 是 `all` 的兼容入口。

已验证的本机版本：ARM GNU 15.2.Rel1、LLVM-MinGW 20261104 / Clang 23.1.5、CMake 4.3.1-msvc1、Ninja 1.13.2、Python 3.14.7。下载 URL、SHA-256 和精确依赖提交见 [dependencies-windows.json](../tools/dependencies-windows.json)。Bootstrap 是公开整理时新增的自动安装入口；完整网络下载与全新电脑安装尚未实测，已配置目录上的工具及构建入口经过本机验证。

## Linux / GitHub Actions

Ubuntu 安装 `cmake ninja-build build-essential python3 curl libusb-1.0-0-dev` 后，使用原有固定工具链安装脚本：

```sh
bash tools/install_toolchain.sh
export PICO_SDK_PATH="$HOME/pico-sdk"
export PATH="$HOME/.local/opt/arm-gnu-toolchain-15.2.rel1/bin:$PATH"

cmake -S tests/host -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTINYUSB_SOURCE="$PICO_SDK_PATH/lib/tinyusb/src"
cmake --build build/host -j 4
ctest --test-dir build/host --output-on-failure

cmake -S . -B build/prop16 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DPICO_BOARD=pico2_w -DDS5_CLOCK_KHZ=150000 -DENABLE_DS4=ON -DENABLE_SERIAL=OFF -DENABLE_COMPANION_VENDOR=OFF -DOLED_ZH=ON -DOLED_SETTINGS_ROW_HEIGHT=16 -DVERSION="$(cat VERSION)"
cmake --build build/prop16 --target ds5-bridge -j 4
python3 tools/check_flash_reserved.py build/prop16/ds5-bridge-oled.elf build/prop16/ds5-bridge-oled.uf2
bash tools/check_ram_residency.sh build/prop16/ds5-bridge-oled.elf
bash tools/check_xip_setup.sh build/prop16/ds5-bridge-oled.elf 2 11
```

依赖采用 [toolchain.env](../tools/toolchain.env) 固定值；不能为了通过 CMake 任意升级 TinyUSB，因为 RAM 重定位依赖其版本与函数布局。项目 CI 与上述命令一致，不启动原上游桌面应用打包、签名更新源或其他开发板构建。Release 对应版本 tag；不同构建环境的二进制可能存在差异。

## 构建检查

默认逻辑 368、两种渲染各 22、DS4 USB 生命周期 1、原生音频状态 10、USB 描述符 2，合计 425 项。ASan / UBSan 负责主机内存与未定义行为检查，固件额外检查保存区、RAM 热路径、XIP／电压设置。测试不能代替实际 OLED、无线重连和听音验收。

改动配置结构、USB 描述符或音频时，请保留迁移与失败路径测试。发布前检查 `git diff --check`，记录 UF2 SHA-256，确认 Opus / WDL 仍是固定子模块。不要将本机日志中的设备标识、凭据或电脑用户名直接提交。
