# Codex Device Development Guide

这是 `cmaiw82al_ai_toy` 产品工程的开发入口。目标设备是基于 BK7258 /
Armino SMP / CMAiW82AL 开发板的 AI 玩具，第一阶段能力是双眼显示仿真眼睛
动画和 AI 语音交互。

## 源码角色

优先级从高到低：

1. 当前产品工程：`/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy`
2. 已验证硬件文档：`/home/jason/armino1/CMAiW82AL_开发板硬件配置说明_已验证.md`
3. 原理图：`/home/jason/armino1/原理图/`
4. 本产品文档：`/home/jason/armino1/cmaiw82al_ai_toy/docs/`
5. 根目录官方 ai_smp 文档剪藏：`/home/jason/armino1/docs/*.md`
6. 根目录新屏规格书：`/home/jason/armino1/docs/显示屏产品规格书ZTB071T..05.pdf`
7. 旧板适配参考：`/home/jason/armino/cmaiw82al_xiaozhi`
8. 官方 SDK：`/home/jason/armino1/bk_avdk_smp`
9. 上游 Xiaozhi 示例：`/home/jason/armino1/ai_xiaozhi`
10. 官方 WIKI：`/home/jason/armino1/WIKI`

遇到冲突时，不要直接照抄上游 `ai_xiaozhi` 或官方 Beken Genie 默认 GPIO。
先回到当前板实测硬件和产品工程。

## 当前架构

- AP：多媒体和业务主逻辑，入口 `ap/ap_main.c`。
- CP：Wi-Fi、UART0 日志、IPC 和电源保持，入口 `cp/cp_main.c`。
- AI 对话：`ap/main/dialog/`、`ap/main/protocols/`、`ap/components/bk_app_audio/`。
- 双眼显示：`ap/main/ui/`、`ap/main/boards/cmaiw82al_lcd.*`。
- 外部资源：`ap/main/fs/app_vfs.c` 挂载内部 `/if0` 和外部 `/sf0`。

## 当前第二版固件状态

- 第二版 240x240 固件测试仍未稳定：现象是屏幕有条纹，且第二个屏幕不亮。
- SDK 显示链路已有诊断/实验改动：LVGL full flush 等完成、dual SPI 串行化刷新、
  SPI controller 每帧等待完成、`lcd_spi_init_common[%d]` 增加时钟日志。
- 这些改动只能作为定位进度，不能当作稳定结论；下一版不要继续在 240x240
  GC9A01A 上叠加猜测式补丁，优先切到新 160x160 GC9D01 屏做最小点亮验证。

## 当前 240x240 基线约束

- 双屏是两块 GC9A01A 240x240 圆屏，LVGL 逻辑尺寸是 `240x480`。
- LCD2 走 QSPI0 mapping：GPIO22/23/24 + DCX GPIO7。
- LCD1 走 QSPI1 mapping：GPIO2/3/4 + DCX GPIO5。
- 当前 160x160 网表确认两屏独立 reset：LCD2 GPIO6，LCD1 GPIO53。
- 背光是 GPIO25，必须保持 `CONFIG_QSPI_LINE_MODE=1`，避免 QSPI 抢占背光和 DCX。
- 眼睛资源从外部 SPI Flash `/sf0` 读取，默认 `/sf0/neutral.avi`。
- 外部 SPI Flash 使用 SPI0 GPIO14-17，`FLASH_EN` GPIO18 低有效。
- 主麦固定 MIC2/R，`CONFIG_CMAIW82AL_AUDIO_MAIN_MIC=2`。
- 功放 PA/MUTE 是 GPIO8；POWER_LOCK 是 GPIO19。
- 摄像头 GC2145 使用 I2C1 GPIO0/1，PWR GPIO9，RST GPIO28。

## 下一版 160x160 新屏指引

根目录 `docs/` 已新增官方 ai_smp/Beken Genie 开发文档和新屏规格书。下一版固件
的显示目标先按新屏点亮，不要一开始接入完整 AVI/AI 流程。

新屏规格书结论：

- 型号：`ZTB071TBIG05`。
- 尺寸/分辨率：0.71 inch，`160RGB x 160`。
- 驱动 IC：`GC9D01`。
- 接口：`SPI4 LINE`，信号为 `D/C`、`CS`、`SCL`、`SDA`、`RESET`。
- 背光：1 颗白 LED，典型 3.0V/20mA。
- PDF 对 pin6 有不一致：机械图标成 `TE`，Pin Definition 表标成 `GND`。
  未实测前不要启用 TE，也不要把 pin6 当可用同步信号。

官方 ai_smp 文档中和新屏最相关的结论：

- Beken Genie 的官方双屏硬件基线就是 `GC9D01 160x160 x2`。
- 官方 Dual Screen AVI Player 使用双 SPI LCD、`CONFIG_LCD_SPI_GC9D01=y`、
  `CONFIG_LCD_SPI_DEVICE_NUM=2`、RGB565、字节序交换，AVI 资源要求 `320x160`。
- 官方默认资源位置是 SD NAND/`/sd0`，默认 reset 是 GPIO6/GPIO45；这些仍然不能
  直接照搬到 CMAiW82AL，资源和 GPIO 必须回到本板文档核对。
- 官方 App Event、Audio Engine、Network Transfer、Video Engine、Key App、
  LED Blink、Countdown、Factory Config、Motor 文档可作为模块生命周期和 API
  参考，但硬件 GPIO 不作为本板事实来源。

下一版点屏顺序：

1. 先保留本板 GPIO 复用和电源约束，只把 LCD 设备切到 GC9D01 160x160。
2. 建立 LVGL 逻辑尺寸 `160x320`，上半屏/下半屏分别映射到两块物理屏。
3. 先刷静态色块并确认两屏各自可控，日志必须覆盖 LCD 注册、
   `SPI_CTLR open spi_id=0 reset_pin=6 dc_pin=7`、
   `SPI_CTLR open spi_id=1 reset_pin=53 dc_pin=5`、
   `lcd_spi_init_common[0]`、`lcd_spi_init_common[1]`、两路 SPI frame 完成。
4. 条纹优先查像素格式、RGB565 byte swap、`0x36` MADCTL、全窗口
   CASET/RASET/RAMWR 和 SPI 时钟，不先改 AI、音频、网络。
5. 两屏稳定显示静态色块后，再接 LVGL fallback；最后再接 `/sf0` AVI。

## 开发流程

1. 修改前先查 `docs/02_hardware_constraints.md` 和相关源码。
2. 涉及双屏、AVI、资源路径时先查 `docs/03_dual_screen_avi_stability.md`。
3. 只在产品工程内改动；必须改 SDK 时，把原因和补丁沉淀到 `patch/`。
4. 改完先跑静态护栏：

```sh
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

5. 再构建固件：

```sh
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

6. 双屏稳定性不能只靠编译判断，必须上板验证完整启动、空闲、聆听、说话、
   网络重连和资源缺失降级路径。
