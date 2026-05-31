# 双屏和 AVI 稳定规则

本板历史上出现过 AVI 播放卡顿、LCD2 黑屏、白屏和资源打开失败。以后改显示
链路时，先按本文件排查，不要先改云端、音频或协议。

## 当前显示基线

- LCD 设备名：`cmaiw82al_gc9d01_160`
- LVGL：`LCD_WIDTH=160`，`LCD_HEIGHT=2*160`
- 双 SPI：LCD2 是 `spi_id=0`，LCD1 是 `spi_id=1`
- DCX：LCD2 GPIO7，LCD1 GPIO5
- reset：两屏独立 reset，LCD2 GPIO6，LCD1 GPIO53，由
  `APP_LCD_CONFIG()` 的 `.rst_pin` 交给 SDK `bk_lcd_spi_init()` 处理
- 背光：GPIO25
- 静态 fallback：运行时只允许使用安全黑底空白，不能使用点屏阶段的黄蓝诊断
  色块。AVI 已经显示过有效帧后，错误路径优先保持上一帧，避免把 fallback 闪给
  用户。

## 第二版测试进度

- 第二版 240x240 固件测试现象：屏幕有条纹，且第二个屏幕不亮。
- 当前 SDK 显示链路已有诊断/实验改动：LVGL full flush 等待显示完成，
  dual SPI 改为先刷 LCD1 再刷 LCD0，SPI controller 在每帧后等待底层完成，
  `lcd_spi_init_common[%d]` 打印 device/reset/DCX/clk/inited。
- 这些改动说明排查已经推进到 LVGL flush、dual SPI split、SPI 队列、DMA 等
  数据路径层；但没有长测通过前，不能把它当作稳定修复。
- 继续排查 240x240 时，条纹优先查像素格式、byte swap、MADCTL、全窗口
  CASET/RASET/RAMWR、SPI 时钟和帧大小；第二屏不亮优先查 LCD1/LCD2 split、
  SPI0/SPI1 frame done 和对应 DCX/CS 线。

## 下一版 160x160 GC9D01 点屏规则

新屏 `ZTB071TBIG05` 是 160x160 GC9D01，和官方 Beken Genie 双屏基线一致。
下一版先点亮新屏，再接 AVI 和 AI 状态。

- LCD 目标：`GC9D01 160x160 x2`。
- LVGL 逻辑尺寸：`LCD_WIDTH=160`，`LCD_HEIGHT=2*160`。
- 帧大小：单屏 `160*160*2` 字节，双屏 full frame `160*320*2` 字节。
- 官方可参考：SDK `lcd_spi_gc9d01.c` 初始化表、Dual Screen AVI Player 的
  RGB565 byte swap 和 `320x160` AVI 资源要求。
- 本板仍需保留：GPIO 复用、DCX GPIO7/GPIO5、reset GPIO6/GPIO53、背光 GPIO25、
  `/sf0` 资源路径，除非新硬件实测推翻。
- 点屏 bring-up 阶段可以临时刷静态双色块和文字；确认两屏均可独立刷新后，再
  恢复产品 LVGL fallback，最后恢复 AVI。产品运行固件不能保留黄蓝诊断色块。

## AVI 和资源规则

- 默认眼睛文件是 `/sf0/neutral.avi`。
- `/sf0` 是外部 SPI Flash littlefs，由 `app_vfs_init()` 挂载。
- `all-app.bin` 不包含 `/sf0` 资源；只烧主固件后没有眼睛动画是预期风险。
- 当前开发固件需要禁止 GPIO54 长按关机和系统 deep sleep 路径，防止长时间 AVI
  调试或 OTA 前后误触发释放 GPIO19 `POWER_LOCK`。若日志出现
  `ignore shutdown key event in development build` 或
  `deep sleep blocked in development build`，说明防护生效，GPIO19 会被重新配置为
  输出高电平；发布版再恢复真实关机策略。
- `bk_dual_screen_avi_player` 不应该自己 mount/unmount SD；VFS 由
  `app_vfs_init()` 统一负责。
- AVI 失败不能中止 UI 初始化；应保留 fallback 并打日志。
- 用户看到一闪而过的黄蓝画面，但串口日志没有 `avi jpeg invalid`、
  `jpeg decode failed`、`bk_avi_video_prase`、`rx overflow`、`Fault`，并且
  `EYE_AVI_FPS` 约为 25fps 时，优先检查产品 fallback 是否被切到前台。2026-05-27
  已确认早期点屏诊断 fallback 使用 `0x005BFF`/`0xFFD400`，会在 AVI 切换或错误路径
  被用户看到；产品 fallback 已改为 `UI_SAFE_FALLBACK_COLOR=0x000000` 且清空文字，
  静态护栏禁止黄蓝诊断色回归。
- 串口出现大段十六进制、`stack mem dump`、`Fault on thread hw_dec_thread`
  和 `Memory management fault occurred address is 00000000` 时，优先按
  AVI/JPEG 硬解崩溃处理，不按串口乱码处理。
- SDK `bk_video_player` 在调用 JPEG 硬解前必须校验 AVI 帧长度、实际读取长度、
  JPEG SOI/EOI/SOF0/SOS、尺寸、三组件格式和 DQT0/DQT1。无效帧应打印
  `avi jpeg invalid ... reason=...` 并返回错误，让产品 UI 保留 fallback。
- 若长时间播放后才出现 `reason=missing soi offset=0 len=10432 expected=320x160
  got=0x0`，不要直接认定源 AVI 损坏。已验证的 `neutral.avi` 中 10432 字节首帧
  静态内容是 `FF D8 ... FF D9`，更可能是 `/sf0` 外部 SPI flash 运行时读帧被
  `spi rx overflow` 或总线压力扰动。SDK 应在坏帧路径重新 seek/read 一次，并
  打印 `avi jpeg read diag ... prefix=...` 前 16 字节，用来区分读到了 `00dc`、
  `LIST`、全 0 还是随机数据。
- 若日志显示 `avi jpeg read retry recovered`，但之后仍出现
  `decoder_error, 7347, ...` 和 `avi_player_jpeg_hw_decode_start failed`，说明
  读帧边界重试只能覆盖短读/缺 EOI，问题已经进入硬件 JPEG 解码阶段。SDK 应在
  硬解失败时打印 `avi jpeg decode diag ... hash=... prefix=... tail=...`，并
  对同一帧重新 seek/read/validate 后再硬解一次。当前 `neutral.avi` 中 `len=7347`
  的静态帧 FNV32 是 `0x527cfec8`，尾部应为
  `52 8a f4 c4 7a 75 cd 9b 2f 2f 36 6c d9 b0 ff d9`；上板日志若相同，优先查
  硬解器兼容性/状态或 SPI/LCD/日志压力，若不同则继续查 `/sf0` 运行时读内容变化。
- 若日志已经能 `AVI_OPEN /sf0 stat ... size=...` 并解析出
  `avi video_num: ..., width: 320, height: 160`，但第一帧硬解失败为
  `decoder_error, 9992, 11170, 0, 50` 这类 `master_rd_cnt > src_size` 形态，
  不要在产品默认配置里直接打开 `CONFIG_JPEGDEC_HW_SUPPORT_FFD9_CHECK`。2026-05-27
  上板实验显示，打开该硬件 `FF D9` 检查后固件卡在 CP 侧
  `IPC retry to start core1`，AP 没有完成启动握手。该选项只能在隔离工程里单独
  验证，主线先保持禁用，继续从资源编码、软件解码 fallback 或 AVI 启动延后排查。
- 眼睛资源体验目标保持 `20-25fps`。用户体验已确认不建议低于 `20fps`，因此
  不要把 `-r 15` 当成首选修复；优先修外部 SPI 读稳定性、坏帧重读、日志限流、
  SPI baud/调度压力。
- 2026-05-27 阶段收口：本地 `ffprobe`/`ffmpeg` 已验证
  `resources/eyes/bk_avi_320x160_sf0_build/sf0` 下 18 个表情 AVI 都是
  `320x160`、`25fps`，并能被 ffmpeg 完整解码。上板串口轮播日志
  `/tmp/eye-cycle-20260527-232211.log`、
  `/tmp/eye-targeted-20260527-232525.log` 和
  `/tmp/eye-suspect-20260527-232811.log` 未出现 `Fault`、`Assert`、重启或串口
  掉线，CP 心跳持续 `POWER_LOCK(GPIO19)=1`。当前可临时收口的边界是：资源文件
  本身不是主要问题，运行时硬件 JPEG 仍普遍失败并走软件 fallback，实测 FPS 多在
  `16fps` 左右，`tired.avi` 曾出现 `avg=14.70` 的边界样本；这满足继续推进
  姿态/触摸/马达/语音开发的稳定性要求，但还不是 AVI 性能最终优化完成。当前默认
  表情应优先使用已确认有首帧发布的 `happy`、`neutral`、`smiling`、`sad`、
  `angry`、`afraid`、`caring`、`confused`、`curious`、`doubtful`、`frowning`、
  `pensive`、`suprised`、`tired`、`winking`；`bored`、`grimacing`、`photo`
  在 25 秒窗口内只看到 `AVI_OPEN`，未看到 `EYE_AVI_FLUSH`，暂不放入核心默认
  轮播，后续再查首帧发布/硬解兼容性。
- 2026-05-28 重新核对根目录 `/home/jason/armino1/bk_avi`：所有 AVI 都是
  `mjpeg 320x160 yuvj422p 25fps`。已通过板端 `update eyes` 写入正式
  YUV422 镜像
  `output/2026-05-26-sf0-yuv422-repack/sf0_bk_avi_320x160_yuv422_repack_15m.bin`
  （15MiB，SHA256
  `75a8d27bcd90d42a01ec35ae05ab18eef082d39c48ab384b9dd31bfbcac017bc`）。
  串口日志
  `logs/serial/20260528-094904-ttyUSB0-115200.log` 证明写入完成：
  `EYES_UPDATE progress: 15728640/15728640`、`remount /sf0 success`、
  `EYES_UPDATE /sf0 stat path=/sf0/neutral.avi size=283702`。重启后
  `/sf0/happy.avi size=283702`，`avi video_num: 28, width: 320, height: 160,
  frame_size: 102400, fps: 25`，后续段统计 `hw_after_success=72`、
  `hw_after_fail=0`、`software fallback decode success=0`、`jpeg decode failed=0`，
  `EYE_AVI_FPS` 稳定在 `avg=24.99-25.00`。因此官方反馈的硬解资源格式问题已
  被验证：旧资源导致硬解失败，正式 YUV422 资源可走硬解并满足 15fps 以上目标。
- 2026-05-28 10:00 复测用户用 BK 官方 AVI 软件重新生成并覆盖
  `/home/jason/armino1/bk_avi` 的资源：当前该目录所有 AVI 变为
  `mjpeg 320x160 yuvj420p 25fps`，不是 YUV422。已生成并写入
  `output/2026-05-28-bk-official-avi-yuv420-test/sf0_bk_official_avi_yuv420_15m.bin`
  （15MiB，SHA256
  `5864ba6208b4599f0f49a721a0864eb81d9b706cbe8c7850f7ab3b532d65053a`），
  `EYES_UPDATE progress: 15728640/15728640`、`remount /sf0 success`、
  `/sf0/neutral.avi size=210936`。重启后 `/sf0/happy.avi size=210936`，
  `avi video_num: 28, width: 320, height: 160, frame_size: 102400, fps: 25`，
  但后续统计为 `hw_after_success=0`、`hw_after_fail=63`、
  `software fallback decode success=1225`、`jpeg decode failed=1430`、
  `software_jpeg_decode_ctlr_decode ... failed=90`，`EYE_AVI_FPS` 平均约
  `14.56fps`。这版资源可作为 YUV420/软解回退压力样本，但不能作为硬解通过
  样本；若目标是验证硬解和消除闪帧，应继续使用或重新生成 YUV422 MJPEG AVI。
- 2026-05-28 阶段闭合：AVI 眼神播放进入可交互开发基线。当前闭合结论是：
  闪屏不是 320x160 分割到两块 160x160 屏幕的显示切片错误，主要由资源格式、
  外部 `/sf0` 读稳定性、硬解兼容性和早期诊断 fallback 混合触发。使用用户确认的
  YUV422 MJPEG 资源镜像后，板端日志在轮播段显示 `EYE_AVI_FPS avg=24.99-25.04`、
  `hw_after_fail=0`、`software fallback decode success=0`、`avi jpeg invalid=0`，
  且用户物理观察确认 `happy`、`photo` 和轮播表情未再出现异常闪帧。后续 IMU
  阶段曾回到纯 `/sf0` 流式读取，实测自然播放降到 `avg=13.48-13.87`，并再次出现
  `/sf0` `short read`。当前运行基线改为：`video_osi_wrapper.c` 只对只读
  `/sf0/*.avi` 启用 manifest 校验后的 PSRAM 整文件 cache；必须命中
  `/sf0/avi_manifest.txt` 中的 size/hash，读取并复算 hash 一致后才启用 cache。
  非 manifest 管理的普通文件保留直接 POSIX VFS fallback；manifest 已知的
  `/sf0/*.avi` 若 cache 分配、读取或 hash 校验失败，必须拒绝本次打开，让 UI
  保持上一帧或安全 fallback，不能退回无保护的 `/sf0` 直接流式读取。
  2026-05-28 日志
  `logs/serial/20260528-194308-ttyUSB0-115200.log` 已看到
  `VIDEO_OSI avi cache enabled path=/sf0/happy.avi size=283702 hash=0xa0154380`，
  后续 `EYE_AVI_FPS avg=24.99-25.07`、`errors=0`、`overruns=0`。追加 55 秒复测
  `logs/serial/20260528-194800-ttyUSB0-115200.log` 显示连续
  `EYE_AVI_FPS avg=24.97-25.00`，无 `avi jpeg invalid`、`short read`、
  `hw decode failed`、`Fault` 或 `Assert`。后续不允许把该 manifest-verified
  PSRAM cache 回退成无保护的纯流式读取。
- 2026-05-28 语音状态桥验证日志
  `logs/serial/20260528-223525-ttyUSB0-115200.log` 显示，在 `pet voice connect`
  建立 WebSocket、再按 `record_start/record_stop/tts_start/tts_stop/abort/idle`
  快速切换表情后，`confused.avi` 稳定段连续 `EYE_AVI_FPS avg=24.99-25.04`、
  `errors=0`、`overruns=0`。该轮没有 `avi jpeg invalid`、`jpeg decode failed`、
  `EYE_FRAME_COLOR_ALERT`、`Assert at` 或 `HardFault`。快速切换过程中若出现
  manifest cache 重试或 hash mismatch，但随后 `VIDEO_OSI avi cache enabled`
  且稳定播放恢复 25fps，应视为可恢复的 `/sf0`/PSRAM cache 切换压力；业务层
  表情切换需要节流，避免连续高频请求。
- 2026-05-29 IMU monitor 回归验证日志显示，`pet motion monitor on` 后任务以
  `180ms` 周期运行，`monitor=1`。监听日志
  `logs/serial/20260529-090536-ttyUSB0-115200.log` 覆盖约 70 秒自然播放，
  `EYE_AVI_FPS` 连续为 `avg=24.98-25.00`、`errors=0`、`overruns=0`，未见
  `EYE_FRAME_COLOR_ALERT`、`avi jpeg invalid`、`jpeg decode failed`、
  `short read`、`hw decode failed`、`Assert`、`HardFault` 或 `rtos_dump`。
  关断日志 `logs/serial/20260529-090654-ttyUSB0-115200.log` 显示
  `monitor disabled` 和 `pet motion ready=1 monitor=0`。这证明低频手动 IMU
  monitor 不会把当前 AVI cache 基线拖回 15fps 以下；真实晃动压力测试仍需单独做。
- 2026-05-29 已针对 IMU monitor 自动路由补表情节流，避免交替姿态事件按
  `180ms` 采样周期高频切换 AVI。节流版固件 OTA 日志
  `logs/serial/20260529-092442-ttyUSB0-115200.log` 显示 OTA 下载到
  `cyg_recvlen_per:(100.00)%`、`write over` 并软件重启；产物保存在
  `output/2026-05-29-pet-motion-route-throttle/`。重启后 `happy.avi` 命中
  manifest PSRAM cache，`logs/serial/20260529-093055-ttyUSB0-115200.log`
  覆盖 monitor 开启后的空闲播放，`EYE_AVI_FPS avg=24.96-25.00`、
  `errors=0`、`overruns=0`，未出现自动 `route motion=` 洪泛。关断日志
  `logs/serial/20260529-093417-ttyUSB0-115200.log` 显示 `monitor disabled`，
  后续播放仍保持 `avg=24.99-25.00`。该结果确认节流补丁和 AVI cache 空闲共存；
  真实手持晃动/倾斜/冲击压力测试仍需补充。
- 2026-05-29 `caring.avi` 复测确认 cache-required 保护生效。此前串口重插后
  `caring.avi` 在 cache 失败时退回 `/sf0` 流式读取，FPS 长期约 `5-11fps`，
  且并发 `eye verify hash` 会放大 `/sf0` 短读。修正后
  `video_osi_wrapper.c` 对 manifest 已知 AVI 在 cache 读取/hash 失败时返回
  `VIDEO_OSI_AVI_CACHE_REQUIRED`，`f_open_wrapper()` 打印
  `VIDEO_OSI avi cache required ... reason=cache required` 并关闭 fd，不再把
  `caring.avi` 作为普通 POSIX 文件打开。证据：
  `logs/serial/20260529-125130-ttyUSB0-115200.log` 出现两轮
  `VIDEO_OSI avi cache required path=/sf0/caring.avi reason=cache required`，
  没有 `avi jpeg invalid`、`short read`、`jpeg decode failed` 或低 FPS
  `EYE_AVI_FPS`；随后 `logs/serial/20260529-125231-ttyUSB0-115200.log` 显示
  `caring.avi` 连续 `EYE_AVI_FPS avg=24.99-25.00`、`errors=0`、`overruns=0`。
  后续不允许把 manifest 已知 AVI 的 cache-required 打开失败改回直接流式播放。
- 2026-05-30 触摸短按两路马达验证通过后，用户仍观察到首次切换眼神卡顿约
  `2s`。串口证据显示同文件 `happy.avi` 已通过 `EYE_SWITCH skip current`
  跳过重开，卡顿根因集中在首次切到 `curious.avi` 时同步读取 `/sf0` 整个 AVI、
  复算 manifest hash 并重试。修正方向不是改触摸或马达，而是保留
  manifest-verified PSRAM cache 基线，同时将 cache 数据做成可复用的持久条目；
  启动默认 `happy.avi` 后，低优先级后台只预热触摸/聆听热路径 `curious.avi`。
  试验曾预热 `pensive.avi` 和 `smiling.avi`，但较大的 `/sf0` 整文件读取会在启动后
  拉低当前眼神 FPS，因此暂不做多表情启动预热。验收日志应看到
  `VIDEO_OSI avi cache persistent stored`、预热 `EYE_PREHEAT file=... ret=0`，
  后续触摸/语音状态切换命中 `VIDEO_OSI avi cache persistent hit`，避免在交互回调
  首次付出整文件读 flash 的几秒成本。
- 2026-05-30 后续 CLI 复现 `pet event touch_chin_short` 显示，`curious.avi`
  已命中 `VIDEO_OSI avi cache persistent hit`，从 `EYE_SWITCH request` 到
  `bk_avi_player_open complete` 约 `0.33s`，不再是 `/sf0` 整文件缓存加载问题。
  为了继续定位用户体感的 `2s` 卡顿，UI 切换路径新增
  `EYE_SWITCH_LATENCY file=... queued=... open=... decode=... publish=... display=...
  total=... ret=...`，后续物理触摸复测必须以该日志拆分 request 排队、打开、
  首帧解码、首帧 flush 和切到 AVI 层的耗时，避免继续靠肉眼判断卡顿阶段。
- 2026-05-30 进一步修正预热语义：`bk_video_osi_preload_avi_cache()` 只有在
  `f_open_wrapper()` 返回真实 AVI cache handle 时才返回 `BK_OK`，否则打印
  `VIDEO_OSI avi preload cache miss` 并返回失败；UI 预热线程按
  `EYE_AVI_PREHEAT_ATTEMPTS` 重试并打印
  `EYE_PREHEAT file=... attempt=... ret=...`。上板日志显示 `curious.avi` 第一次
  `/sf0` 读取短读，第二次 SDK 内部 cache attempt 成功
  `VIDEO_OSI avi cache persistent stored path=/sf0/curious.avi`，之后
  `EYE_PREHEAT file=curious.avi attempt=1 ret=0`。CLI 再触发
  `pet event touch_chin_short` 时命中 persistent cache，`EYE_SWITCH_LATENCY`
  为 `queued=15 open=204 decode=10 publish=8 display=1 total=241 ret=0`。
  因此，当前触摸表情切换的 2 秒体感不应继续归因于 `curious.avi` 文件打开；
  若物理触摸仍卡顿，应先确认日志是否出现 `touch_*`、`pet_brai`、
  `pet_hapt` 和对应 `EYE_SWITCH_LATENCY`。本轮物理触摸配合窗口未捕获到这些
  事件，说明还需要单独复测触摸输入链路。
- 下一阶段交互开发默认可以依赖以下接口和行为：启动默认播放 `EYE_AVI_BOOT_NAME`
  (`happy.avi`)；聊天/状态通过 `app_ui_display_chat_emotion()` 映射到表情 AVI；
  串口保留 `eye list|play <avi>|carousel [seconds]|stop|verify` 作为诊断入口；
  资源更新使用 `update eyes <url>` 写入外部 SPI `/sf0`；主固件 OTA 使用
  `update ota <url>`。若 `/sf0` 缺失、AVI 打不开或启动首帧失败，UI 只能显示黑色
  safe fallback 或保持上一帧，不允许回归黄蓝点屏诊断画面。
- 仍然挂起的低优先级项：当前部分诊断固件里 `AVI_JPEG_MEM` 日志较密，可能拉低
  串口统计 FPS；官方流式读路径下偶发 `avi jpeg invalid`/短读重试仍需要长期观察。
  这些不阻塞姿态传感器、触摸、振动马达和 AI 语音对话开发，但在发布前需要做一次
  去诊断日志后的长测，目标仍是无闪屏、无 `Fault`/`Assert`、平均 FPS 不低于 15。
- 若日志显示 `avi video_num: ..., width: 320, height: 160` 后紧接
  `reason=missing dqt table`，说明 AVI 容器尺寸正确，但 JPEG 帧只有 DQT0，
  不适合当前硬解路径。应重刷外部资源镜像，例如
  `output/2026-05-26-sf0-dqt-fixed/sf0_bk_avi_320x160_dqt_fixed_15m.bin`。
- 若日志显示 `AVI_OPEN /sf0 stat fail path=/sf0/neutral.avi ret=-1`，同时已经有
  `mount spi flash littlefs success start=0x0 size=0xf00000`、两路 `SPI_CTLR open`
  和 `LVGL_FLUSH full`，说明 LCD 数据链路已通，但当前挂载的外部 `/sf0`
  根目录没有 `neutral.avi`。这通常是资源镜像没有写到外部 SPI flash 的
  offset `0x0`，或不是按 15MiB littlefs 原始镜像写入。优先用板端
  `update eyes http://<host>/sf0_bk_avi_320x160_dqt_fixed_15m.bin`，成功日志应包含
  `EYES_UPDATE content_length=15728640`、写入进度、`remount /sf0 success` 和
  `EYES_UPDATE /sf0 stat path=/sf0/neutral.avi size=...`。
- 若日志已经包含 `HTTP/1.1 200 OK`、`Content-Length: 15728640`、
  `EYES_UPDATE progress: 15728640/15728640` 和
  `update flash image written: 15728640 bytes`，但重挂载后仍
  `EYES_UPDATE /sf0 stat fail path=/sf0/neutral.avi ret=-1`，不要再优先怀疑 HFS
  地址或 HTTP 下载。此时失败边界在外部 SPI flash 写后可见的 littlefs 内容；
  新固件会在 stat 失败时打印 `EYES_UPDATE /sf0 dir entry[...]` 和
  `EYES_UPDATE flash[0..31]=...`，用来区分目录内容不匹配、镜像头未写入或 flash
  写入/擦除异常。
- 若同一轮日志中 CLI/update 已打印
  `EYES_UPDATE /sf0 stat path=/sf0/neutral.avi size=...`，但 UI 立即打印
  `AVI_OPEN /sf0 stat fail path=/sf0/neutral.avi ret=-1`，说明资源镜像、HTTP
  下载、外部 SPI flash 写入和 `/sf0` 挂载都已经通过，失败边界在 UI 模块的
  VFS API 映射。所有在产品模块里对 `/sf0` 使用 `stat()`、`opendir()`、
  `readdir()` 等 POSIX 风格接口的源文件，都必须包含 `bk_posix.h`，确保走
  BK VFS，例如 `stat(path, buf)` 映射到 `bk_vfs_stat(path, buf)`。
- 若上述 `stat fail` 之后又出现 `event: 12`、`ui disp avi` 和新的
  `LVGL_FLUSH full`，但屏幕变白或空白，优先判断为 AVI 图层被切到前台而没有
  有效帧数据。固件应在 AVI 未打开或 `avi_desc.data == NULL` 时保持静态
  fallback 可见，并打印 `AVI unavailable; keep static fallback visible`。这仍然
  不是 JPEG 硬解失败；硬解排查至少要等到日志出现 `avi video_num: ... width:
  320, height: 160` 之后。

## 故障排查顺序

1. 查 GPIO25 背光是否亮。
2. 查日志是否有 `registered LCD device: cmaiw82al_gc9d01_160`。
3. 查是否有 `SPI_CTLR open spi_id=0 reset_pin=6 dc_pin=7` 和
   `SPI_CTLR open spi_id=1 reset_pin=53 dc_pin=5`。
4. 查 `lcd_spi_init_common[0]` 和 `[1]` 是否都出现。
5. 查 `/sf0` 是否挂载成功，文件是否存在且大小正确。
6. 若只有一只屏黑，先看 LVGL flush、dual SPI、SPI controller 和低层 frame
   日志，不要先怀疑 AI 协议。
7. 若 AVI 卡顿或长测出现坏帧，先查 SPI overflow、DMA busy、JPEG decoder
   错误、`avi jpeg read diag` 前缀和 PSRAM slab 使用。不要先把资源降到
   15fps；必要时优先做外部 SPI flash 降速、日志限流或坏帧重读。
8. 若日志显示 `Fault on thread hw_dec_thread`，用 `addr2line` 解析 backtrace。
   若落在 `JpegdecInit`/`hw_jpeg_decode_thread`/`bk_avi_player_video_parse`，
   检查 `/sf0/neutral.avi` 的帧格式和 `avi jpeg invalid` 前置校验日志。

## 2026-05-27 救援基线

用户观察到的“经常自动关机”在串口日志中对应 AP assert/CP nmi watchdog，而不是
正常定时关机路径。关键证据：

- 异常日志包含 `stack mem dump begin`、`rtos_dump_system:*`。
- 重启原因包含 CP `reason - nmi watchdog` 和 AP `reason - assert`。
- CP 心跳仍打印 `POWER_LOCK(GPIO19)=1`，电源保持脚没有被正常关机逻辑释放。
- dump 内存列表中存在 `eye_carousel`，并且持有多块 `102400` 字节 AVI buffer，
  同时有 `avi_player_thread`、`hw_dec_thread`、`lvgl` 等显示/解码任务。

因此，当前优先按 AVI/显示链路崩溃处理，而不是按电源按键、deep sleep 或定时
关机处理。为了保住 OTA 和串口窗口，救援固件默认不在 `app_ui_init()` 中打开
AVI，也不默认启动 `eye_carousel`；启动后只显示静态 fallback。后续需要通过串口
命令手动启动眼神：

```text
eye play neutral.avi
eye carousel 6
eye stop
```

若板子已经处于异常 dump 或无输出状态，在此状态下发送 `update ota ...` 不可靠：
命令可能发在 shell 创建之前，后续 AP/CP 重启后不会自动重发。此时应先手动烧录
救援 `all-app.bin` 一次；救援固件启动稳定后，再恢复 OTA 工作流并继续定位 AVI
闪帧/FPS。

## 官方默认的适用范围

官方 Beken Genie 文档和上游示例描述的是 GC9D01 160x160、SD NAND、
`/sd0/genie_eye.avi` 和独立 reset 脚。对下一版新屏，GC9D01 160x160 和
`320x160` AVI 资源要求可以作为参考；但 SD NAND、`/sd0`、GPIO6/GPIO45 reset
和参考板按键/LED/马达 GPIO 仍不能直接套用。CMAiW82AL 的资源路径、GPIO 和
电源约束以产品文档为准。

## 上板验收

编译通过不等于显示稳定。每次改显示链路后至少验证：

- 冷启动和软件重启。
- 空闲显示 5 分钟以上。
- 唤醒、聆听、思考、说话状态切换。
- Wi-Fi 断开/重连。
- `/sf0` 资源缺失或 AVI 打不开时，只显示安全 fallback 或保持上一帧，不能出现
  黄蓝诊断色块。
- 两只屏都持续刷新，没有单屏黑屏。
