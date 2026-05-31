# 官方 ai_smp 文档和 160x160 新屏指引

本文件沉淀 2026-05-26 已通读的根目录 `docs/*.md` 官方文档，以及
`显示屏产品规格书ZTB071T..05.pdf` 对下一版固件的约束。它是开发指引补充层：
官方文档用于理解模块生命周期和 GC9D01 160x160 双屏参考实现，本板 GPIO、
电源、存储和资源路径仍以产品工程文档为准。

## 已读官方文档

| 文档 | 可用于本项目的结论 |
| --- | --- |
| `Beken Genie AI` | 官方 AI toy 基线是双 SPI LCD `GC9D01 160x160 x2`、音频 AEC/NS、G711/G722、KWS、BLE 配网、DVP 图传；UI 资源要求 AVI 转换后 `320x160`。 |
| `Beken Genie火山RTC版本` | 和 Genie AI 架构相同，但 RTC 后端换成 VolcEngine；双屏、资源和状态机结论一致。 |
| `Dual Screen AVI Player 模块` | 双屏 AVI 链路为 AVI Player -> `bk_display_dual_spi` -> 两块 SPI LCD；默认配置是 `lcd_device_gc9d01`、`spi_id=0/1`、DCX GPIO7/GPIO5、reset GPIO6/GPIO45、RGB565 byte swap。 |
| `App Event 模块` | 模块间事件必须走队列/回调；回调不能长时间阻塞。显示状态切换应保持事件化，不要在协议或音频回调里直接做重显示工作。 |
| `Audio Engine 模块` | 音频引擎基于 Voice Service，采样率只支持 8000/16000；PA GPIO、AEC/NS、编码器和回调要从本板配置落地。 |
| `Network Transfer 模块` | 网络传输抽象层在 Agora/Volc RTC 后端上封装音频、视频和 Agent 生命周期；发送前检查启动状态，回调不能阻塞。 |
| `Video Engine 模块` | DVP/UVC 摄像头通过帧队列和传输任务送到 Network Transfer；图传调试不要和 LCD 点亮混在一轮改动里。 |
| `Key App 模块` | 按键事件从 GPIO/multi_button 转为系统事件；本板按键 GPIO 不能照搬官方表。 |
| `LED Blink 模块` | LED 用状态投票和优先级管理；仅作状态提示参考，本板 LED GPIO 另查硬件约束。 |
| `Countdown 模块` | 配网/网络错误/待机倒计时会进入深睡，OTA 票暂停倒计时；调屏长测时注意关闭或延长倒计时。 |
| `Factory Config 模块` | 配置写入先到 SRAM cache，必须 sync 才持久化；适合保存音量、Agent 信息等，不用于 LCD 硬件事实。 |
| `Motor 模块` | 官方 PWM 马达方案不适用本板 MS32008N1 I2C 步进驱动，只能参考生命周期。 |

## 新屏规格结论

规格书：`/home/jason/armino1/docs/显示屏产品规格书ZTB071T..05.pdf`

| 项目 | 结论 |
| --- | --- |
| 模组 | `ZTB071TBIG05` |
| 分辨率 | `160(H)RGB x 160(V)` |
| 尺寸 | 0.71 inch，Active area `18 x 18 mm` |
| 显示模式 | Normally black |
| 接口 | `SPI4 LINE` |
| 驱动 IC | `GC9D01` |
| 颜色排列 | RGB vertical stripe |
| 典型亮度 | 350 cd/m2 |
| 背光 | 1 white LED，典型 3.0V，最大 20mA |
| 工作温度 | -20 到 +70 C |
| 存储温度 | -30 到 +80 C |

12pin 信号：GND、LEDK、LEDA、VDD、GND、pin6、D/C、CS、SCL、SDA、RESET、GND。
PDF 机械图把 pin6 标为 `TE`，Pin Definition 表把 pin6 标为 `GND`。未实测前
不要启用 TE，不要把 pin6 当作可用同步输入。

## 和当前第二版固件的关系

当前第二版仍是 240x240 GC9A01A 双屏基线，测试现象是屏幕有条纹且第二个屏幕
不亮。SDK 里已有显示诊断和刷新顺序实验，包括 LVGL 等待显示完成、dual SPI
先刷 LCD1 再刷 LCD0、SPI controller 每帧等待 DMA 完成、`lcd_spi_init_common`
打印时钟。这些是定位线索，不是稳定版结论。

新屏与官方 Beken Genie 的 `GC9D01 160x160 x2` 基线一致。下一版应先回到官方
GC9D01 SPI panel 初始化和 `160x320` LVGL 逻辑尺寸，再叠加 CMAiW82AL 的 GPIO、
共享 reset、背光和 `/sf0` 资源约束。

## 下一版点亮顺序

1. 只改 LCD 目标，不同时改 AI、音频、网络、摄像头或资源升级。
2. 将产品 LCD 设备切到 GC9D01 160x160。优先复用 SDK
   `ap/components/bk_peripheral/src/lcd/spi/lcd_spi_gc9d01.c` 的初始化表。
3. 更新产品宏到 `LCD_WIDTH=160`、`LCD_HEIGHT=2*160`，设备名使用新的产品名，
   例如 `cmaiw82al_gc9d01_160`，避免和 SDK 内置 `"gc9d01"` 混淆。
4. 保持本板双 SPI 映射：LCD2 `spi_id=0` + DCX GPIO7，LCD1 `spi_id=1` + DCX
   GPIO5；reset 仍先按本板共享 GPIO53 处理，除非新转接板实测为独立 reset。
5. 点亮第一轮只刷静态色块：上半屏/下半屏使用不同纯色和文字，禁用 AVI。
6. 日志验收必须同时看到：LCD 注册、shared reset、`lcd_spi_init_common[0]`、
   `lcd_spi_init_common[1]`、LVGL flush、dual SPI split、SPI0/SPI1 frame done。
7. 若出现条纹，先查 RGB565 byte swap、MADCTL `0x36`、CASET/RASET 全窗口、SPI
   时钟和帧大小 `160*160*2`。不要先改 `/sf0`、WebSocket、KWS 或音频。
8. 静态色块双屏稳定后，再恢复 LVGL fallback；最后接 AVI。AVI 资源需要转换为
   `320x160`，本板路径仍优先 `/sf0/neutral.avi`，不是官方 `/sd0`。

## 不能照搬的官方默认

- 官方 reset GPIO6/GPIO45 只适用于 Beken Genie 参考板。
- 官方 SD NAND 和 `/sd0` 资源路径不适用于本板当前外部 SPI Flash `/sf0`。
- 官方按键、LED、马达 GPIO 只作 API 示例，不作 CMAiW82AL 硬件事实。
- `CONFIG_LCD_SPI_GC9D01=y` 是 SDK 编译开关，可以沿用；运行时产品设备名、尺寸、
  GPIO 和 reset 仍必须在产品工程里显式定义并受静态护栏保护。
