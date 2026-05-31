# 硬件约束

本文件摘录当前产品工程必须遵守的 CMAiW82AL 板级约束。完整来源是
`/home/jason/armino1/CMAiW82AL_开发板硬件配置说明_已验证.md` 和原理图。

## 核心结论

| 功能 | 当前结论 |
| --- | --- |
| SoC | BK7258，Armino SMP AP/CP |
| 双屏 | 两块 GC9D01 160x160 圆屏 |
| LVGL 逻辑尺寸 | `160x320`，上半屏 LCD2，下半屏 LCD1 |
| LCD2 | QSPI0 mapping：GPIO22 CLK、GPIO23 CS、GPIO24 IO0/SDA、GPIO7 D/C、GPIO6 RESET |
| LCD1 | QSPI1 mapping：GPIO2 CLK、GPIO3 CS、GPIO4 IO0/SDA、GPIO5 D/C、GPIO53 RESET |
| LCD reset | 两屏独立 reset；LCD2 GPIO6，LCD1 GPIO53 |
| LCD 背光 | GPIO25 |
| QSPI line mode | 必须 `CONFIG_QSPI_LINE_MODE=1` |
| 外部 SPI Flash | SPI0 GPIO14-17，GPIO18 `FLASH_EN` 低有效 |
| 眼睛资源 | `/sf0/neutral.avi` 起步，`/sf0` 不在 `all-app.bin` 中 |
| 主麦 | MIC2/R，`CONFIG_CMAIW82AL_AUDIO_MAIN_MIC=2` |
| PA/MUTE | GPIO8 |
| POWER_LOCK | GPIO19，CP 启动后拉高 |
| GC2145 | I2C1 GPIO0/1，PWR GPIO9，RST GPIO28 |
| 振动马达 | CN3 2pin 直流振动马达，走 MS32008N1 OUT5A/OUT5B；不是 J2/J3 5pin 步进电机接口 |

## 160x160 新屏网表结论

`/home/jason/armino1/原理图/ali原图网表_2026-05-26.net` 是当前 LCD
线序优先证据；当旧“共享 reset”文档和网表冲突时，以该网表为准。

新屏规格书和官方 ai_smp 文档给出的共同基线：

| 功能 | 当前目标 |
| --- | --- |
| LCD 型号 | GC9D01 |
| 单屏尺寸 | 160x160 |
| LVGL 逻辑尺寸 | `160x320`，上半屏 LCD2，下半屏 LCD1 |
| 接口 | 4-wire SPI：D/C、CS、SCL、SDA、RESET |
| 背光 | 1 white LED，典型 3.0V/20mA |
| 官方参考 | `CONFIG_LCD_SPI_GC9D01=y`、`CONFIG_LCD_SPI_DEVICE_NUM=2` |

已由网表确认：

- `FL_QSPI_CLK`：U2-P22/GPIO22 -> LCD2-9。
- `FL_QSPI_CS`：U2-P23/GPIO23 -> LCD2-8。
- `FL_QSPI_D0`：U2-P24/GPIO24 -> LCD2-10。
- `LCD_QSPI_D3`：U2-P7/GPIO7 -> LCD2-7，作为 LCD2 D/C。
- `LCD_QSPI_D2`：U2-P6/GPIO6 -> LCD2-11，作为 LCD2 RESET。
- `LCD_QSPI_CLK`：U2-P2/GPIO2 -> LCD1-9。
- `LCD_QSPI_CS`：U2-P3/GPIO3 -> LCD1-8。
- `LCD_QSPI_D0`：U2-P4/GPIO4 -> LCD1-10。
- `LCD_QSPI_D1`：U2-P5/GPIO5 -> LCD1-7，作为 LCD1 D/C。
- `LCD_RST`：U2-P53/GPIO53 -> LCD1-11，作为 LCD1 RESET。
- `LED_BL` 连接 LCD1-2/LCD2-2，驱动链路由 U2-P25/GPIO25
  `LCD_PWM_BL` 通过背光三极管控制。
- `OUT5A`：U4-8/MS32008N1-OUT5A -> CN3-2。
- `OUT5B`：U4-10/MS32008N1-OUT5B -> CN3-1。
- `nSLEEP`：U4-6/MS32008N1-nSLEEP -> U2-P50/GPIO50，经 R37 上拉到控制脚；
  `pet_haptic` 负责显式唤醒、停止、拉低休眠，避免误振动。
- `M_SCL/M_SDA`：U2-P20/P21 连接 MS32008N1 SCL/SDA，当前 I2C 地址按
  `ALI_HAPTIC_MOTOR_I2C_ADDR=0x10` 记录；不在业务代码中直接写 MS32008N1。
- J2/J3 仍是 5pin 步进电机接口；振动反馈只按 CN3 2pin 直流振动马达封装。
- 2026-05-29 已按 MS32008N1 手册和上板测试确认 OUT5 直流输出：
  `chipFlag(0x0f)=0x08`，global `0x00` 写 `0x01` 运行、`0x02` standby/reset，
  `DC_CTRL(0x03)` 写 `0x01` 正转、`0x02` 反转、`0x00` HiZ。
- 验证日志：
  `logs/serial/20260529-202616-ttyUSB0-115200.log` 显示
  `pet haptic probe addr=0x10 ack=1 ret=0 ... chip=0x08`；
  `logs/serial/20260529-202650-ttyUSB0-115200.log` 显示
  `pet haptic dc_test ret=0 requested=60 actual=60 reverse=0 chip=0x08`；
  用户确认 60ms 短振有体感震动；
  `logs/serial/20260529-202733-ttyUSB0-115200.log` 显示测试后
  `awake=0`。

仍需实测确认：

- 规格书 pin6 在不同页面标成 `TE`/`GND`，未确认前不要启用 TE。
- 背光是否仍由 GPIO25 控制，以及电流限制是否满足单屏 20mA、双屏 40mA 量级。

## 关键代码入口

| 约束 | 文件 |
| --- | --- |
| 板级宏 | `ap/main/common/common.h` |
| AP GPIO 复用 | `ap/config/bk7258_ap/usr_gpio_cfg.h` |
| AP config | `ap/config/bk7258_ap/config` |
| CP 电源保持 | `cp/cp_main.c` |
| 外部 Flash 挂载 | `ap/main/fs/app_vfs.c` |
| LCD 注册 | `ap/main/boards/cmaiw82al_lcd.c` |
| 显示初始化 | `ap/main/ui/display_module.c` |
| 静态护栏 | `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py` |

## 禁止回退

- 不要把 PA 改回 GPIO50。
- 不要把 POWER_LOCK 改回 GPIO52。
- 不要把摄像头 PWR 改回 GPIO49。
- 不要恢复 SDIO/NAND 默认 GPIO14-19 复用；这些脚属于外部 SPI Flash。
- 不要恢复 RGB LCD GPIO20-55 默认映射；这些脚被本板其他外设使用。
- 不要把主麦改回 MIC1/L 或 `.main_mic_select = 0`。
- 不要把眼睛资源改回 `/sd0/genie_eye.avi`。
- 不要再按 MS32008N1 五线步进电机设计振动反馈；haptic pattern 按 CN3 2pin 直流振动马达封装。
- 未完成整机功耗、屏幕背光闪烁和交互节流评估前，不要把业务 haptic pattern 默认接到硬件输出；
  只允许通过 `pet haptic dc_test [duration_ms] [reverse]` 做短脉冲诊断。
