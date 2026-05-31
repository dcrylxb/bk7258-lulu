# 上游冲突清单

本文件记录官方文档、上游示例和本板工程的冲突点。以后从 `ai_xiaozhi` 或
官方 WIKI 同步代码时，先检查这里。

## 已确认冲突和换屏后的变化

| 主题 | 上游/官方默认 | CMAiW82AL 产品结论 |
| --- | --- | --- |
| LCD 型号 | GC9D01 | 第二版 240x240 基线是 GC9A01A；下一版新屏目标改为 GC9D01 |
| 单屏尺寸 | 160x160 | 第二版 240x240；下一版新屏目标 160x160 |
| LVGL 逻辑尺寸 | 160x320 | 第二版 240x480；下一版新屏目标 160x320 |
| 眼睛资源 | `/sd0/genie_eye.avi` / SD NAND | `/sf0/neutral.avi` / 外部 SPI Flash |
| LCD reset | GPIO6、GPIO45 独立 reset | GPIO53 共享 reset，一次性 reset |
| LCD2 DCX | 常见样例混用 GPIO6/7 | 当前确定 GPIO7 |
| LCD1 DCX | GPIO5 | GPIO5，且不能被 QSPI 多线模式抢占 |
| QSPI line mode | 可能按多线 QSPI 配 | 必须 `CONFIG_QSPI_LINE_MODE=1` |
| SDIO/NAND | GPIO14-19 用作 SDIO/SD NAND | GPIO14-17 是外部 SPI Flash，GPIO18 是 `FLASH_EN` |
| PA | GPIO50 | GPIO8 |
| 电源保持 | GPIO52/HW_LDO | GPIO19 POWER_LOCK |
| 摄像头 PWR | GPIO49 | GPIO9 |
| 主麦 | MIC1/L 或 `.main_mic_select = 0` | MIC2/R，`CONFIG_CMAIW82AL_AUDIO_MAIN_MIC=2` |

换屏后，官方 GC9D01 160x160 LCD 初始化表、`CONFIG_LCD_SPI_GC9D01` 和
`320x160` AVI 资源要求变成可参考基线；但官方资源路径、reset GPIO、按键、
LED、马达、SD NAND 和充电管理 GPIO 仍是参考板事实，不能直接覆盖本板。

## 具体来源

- `ai_xiaozhi/projects/xiaozhi/ap/main/common/common.h` 仍包含 GPIO52、GPIO50、
  `/sd0/genie_eye.avi`、GC9D01、160x160、GPIO6/GPIO45 reset 等参考板默认。
- `WIKI/Beken Genie AI...` 写明双 SPI LCD 为 GC9D01 160x160，并要求 AVI 放到
  SD NAND。
- `CMAiW82AL_开发板硬件配置说明_已验证.md` 和本产品工程已按实物/原理图修正。

## 处理规则

- 上游可以用于学习 Xiaozhi 协议、系统状态机和组件结构。
- 官方 WIKI 可以用于学习 Armino 构建、Kconfig、分区、外设 API。
- 任何 GPIO、LCD、资源路径、MIC、分区和电源控制都必须回到本产品文档和
  当前工程核对。
- 如果必须临时保留上游名字，例如 `CONFIG_LCD_SPI_GC9D01` 作为 SDK 内置
  SPI LCD 编译开关，文档里要说明它不是运行时设备名。
- 下一版新屏若采用 GC9D01，应使用新的产品设备名和静态护栏保护尺寸/GPIO，
  避免把 SDK 内置 `"gc9d01"` 和产品板级配置混在一起。
