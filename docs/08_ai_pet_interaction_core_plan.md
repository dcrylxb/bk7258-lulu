# AI 电子宠物交互核心计划

本文件把根目录 `BK7258_AI电子宠物交互设计方案_最终版.md` 固化为当前项目主线。
后续开发默认按这里的阶段推进；如果产品方向变化，先更新本文件和对应阶段计划。

## 总目标

BK7258 负责本地宠物身体：眼神、触摸、姿态、震动、本地状态机、隐私/停止、
低功耗和安全策略。Xiaozhi 后端负责云端语音智能：ASR、LLM、TTS、MCP、知识库、
视觉和长期记忆摘要。

当前后端地址：

```text
OTA:  http://106.55.173.79:8989/xiaozhi/ota/
WS:   ws://106.55.173.79:8989/xiaozhi/v1/
MQTT: 106.55.173.79:2883
UDP:  106.55.173.79:8888
```

## 开发边界

- 宠物状态以本地 `pet_brain` 为权威，云端只提交受限行为建议。
- 云端动作必须走 `pet_action_router` 白名单，不能直接控制 GPIO、PWM、flash、
  内存或显示底层。
- 触摸、姿态、跌落、低电、隐私和停止播放必须本地即时响应。
- 离线模式仍要能完成基础互动：摸、抱、晃、安抚、睡觉和眼神变化。
- AVI 眼神播放是当前保护基线。交互阶段只能通过 `app_ui_display_chat_emotion()`
  请求表情，不能改 SDK AVI、JPEG、SPI、LVGL 或 display flush 热路径。
- `/sf0` 眼神资源继续使用已经验证的 `320x160`、`25fps`、YUV422 MJPEG 资源镜像；
  不允许回归黄蓝诊断 fallback。

## 阶段计划

### Phase 1: 本地宠物核心

目标：建立本地状态机、动作路由和串口调试入口。

已落地模块：

- `ap/main/pet/pet_brain.c`
- `ap/main/pet/pet_action_router.c`
- `ap/main/cli/cli_app_pet.c`
- CP UART 转发入口 `cp/cli_update_forward.c`

串口验证入口：

```text
pet status
pet emote <emotion>
pet event <event_name>
pet privacy on|off
pet idle
pet voice connect|record_start|record_stop|tts_start|tts_stop|abort|idle
```

验收标准：

- 静态护栏通过。
- 固件构建通过。
- 物理 UART 上 `pet` 命令经 CP 转发到 AP。
- 支持触摸、隐私、listen、think、speak、abort、idle 等本地状态切换。
- 眼神切换只走 `EYE_SWITCH` 和现有 AVI 播放线程，不出现静态诊断 fallback。
- 2026-05-28 `pet voice` 调试入口已上板验证：`connect` 映射
  `SYSTEM_EVENT_DIALOG_START` 并连接
  `ws://106.55.173.79:8989/xiaozhi/v1/`，服务端返回 `hello`
  和 MCP initialize；`record_start/record_stop/tts_start/tts_stop/abort/idle`
  分别触发 listen、think、speak、abort、idle 状态，日志路径为
  `logs/serial/20260528-223525-ttyUSB0-115200.log`。
- 该版 OTA 产物保存在
  `output/2026-05-28-pet-voice-debug/app_pack.rbl` 和 `all-app.bin`；
  OTA 日志 `logs/serial/20260528-223205-ttyUSB0-115200.log` 显示
  `APP_OTA begin`、`cyg_recvlen_per:(100.00)%`、`write over`、软件重启后
  build time `May 28 2026 21:14:44`。

### Phase 2: 触摸适配

目标：把真实触摸输入映射为标准 pet 事件。

- GPIO46：额头触摸。
- GPIO44：下巴触摸。
- 支持短触、双击、长按和超长按。
- GPIO54 继续保留为电源/恢复键，不作为宠物交互键。

### Phase 3: WebSocket 语音状态桥

目标：把现有对话协议状态接入 `pet_brain`。

- OTA 获取 websocket 配置。
- WebSocket hello 后进入可联网状态。
- listen start/stop、TTS start/stop、abort、cloud error 更新本地状态。
- 云端 emotion 只能通过 `pet_action_router` 规范化后执行。
- OTA 或网络失败时保留本地宠物闭环。

### Phase 4: `pet_action` 和设备侧 MCP

目标：允许云端以受限语义动作驱动设备。

- 解析 `pet_action` JSON。
- 白名单校验 `action` 和 `emotion`。
- 裁剪 `duration_ms` 和 `local_delta`。
- 暴露 `pet_get_state`、`pet_emit_action` 等语义 MCP 工具。
- 禁止暴露原始硬件控制工具。
- 2026-05-29 摄像头 MCP 拍照识图链路已上板闭合。语音触发“看一下前面是什么”
  后，服务端下发 `tools/call self.camera.take_photo`，设备侧 GC2145 单帧抓拍
  `640x480` JPEG，POST 到
  `http://106.55.173.79:8989/xiaozhi/api/vision`，服务端返回 HTTP `200`，
  随后 TTS 回复识图结果。关键日志：
  `logs/serial/20260529-195646-ttyUSB0-115200.log`。
- 同轮验证确认服务端 MCP initialize 当前仍下发
  `https://dashscope.aliyuncs.com/compatible-mode/v1` 作为 vision URL。设备侧必须
  拒绝该模型 API 地址，fallback 到自建后端 `/xiaozhi/api/vision` 上传端点；
  日志显示 `reject invalid vision url`、`use vision fallback url`、
  `find token len:10`，工具列表包含 `self.camera.take_photo`。
- 本地拍照硬件单测日志
  `logs/serial/20260529-194217-ttyUSB0-115200.log` 已确认 GC2145 检测、
  camera open、JPEG SOI `ff d8 ff e0` 正常。后续若拍照识图失败，先按
  “MCP capability -> fallback URL/token -> camera capture -> HTTP upload -> TTS result”
  分层排查，不要直接改 DVP/JPEG。

### Phase 5: 姿态和震动

目标：接入姿态传感器和振动马达，形成完整身体反馈。

- 先确认 IMU 型号、I2C 地址和数据方向，再调阈值。
- 支持抱起、放下、轻晃、剧烈摇晃、倾斜、跌落和冲击。
- 剧烈摇晃、跌落等安全事件优先级高于云端回复。
- 当前振动外设是 CN3 2pin 直流振动马达，网表连接为 MS32008N1 OUT5A/OUT5B；不是 J2/J3 5pin 步进电机接口。
- 不要再按 MS32008N1 五线步进电机设计振动反馈，也不要复用旧 `bk_motor` PWM demo。
- 把 2pin 直流振动马达封装成 haptic pattern，不在业务代码里直接写底层驱动。
- 2026-05-29 已确认 MS32008N1 OUT5 直流输出寄存器与安全启动/停止时序；
  `pet haptic probe` 可读到 `chip=0x08`，`pet haptic dc_test 60` 可驱动
  60ms 短振，测试后 `pet haptic status` 回到 `awake=0`。
- 普通诊断入口 `pet haptic status|stop|<pattern> [duration_ms]` 保留；
  `dc_test` 只是显式硬件短脉冲诊断命令。
- 证据日志：
  `logs/serial/20260529-202616-ttyUSB0-115200.log`、
  `logs/serial/20260529-202650-ttyUSB0-115200.log`、
  `logs/serial/20260529-202733-ttyUSB0-115200.log`。用户已确认马达有体感震动。
- `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 仍保持关闭；短振能力先作为
  CLI 诊断入口，后续完成整机功耗、屏幕闪烁和交互节流评估后，再把业务
  haptic pattern 映射到硬件输出。
- 2026-05-28 上板确认 IMU 是 HXY 兼容器件，GPIO42/43 正常总线地址 `0x19`，`WHO_AM_I=0x11`，version `0x28`；`pet motion probe/sample/route` 已可通过 CP 串口转发验证。
- 2026-05-29 在已恢复的 manifest 校验 PSRAM AVI cache 基线上复测
  `pet motion monitor on`：日志
  `logs/serial/20260529-090510-ttyUSB0-115200.log` 显示 monitor 任务启动、
  周期 `180ms`、`monitor=1`；随后
  `logs/serial/20260529-090536-ttyUSB0-115200.log` 连续约 70 秒保持
  `EYE_AVI_FPS avg=24.98-25.00`、`errors=0`、`overruns=0`；最后
  `logs/serial/20260529-090654-ttyUSB0-115200.log` 显示 `monitor disabled` 和
  `monitor=0`。该验证未出现 AVI/JPEG/Fault/Assert 相关错误。低频手动 monitor
  可作为诊断能力保留，但默认自动姿态行为仍需等真实晃动压力测试和表情节流确认后再开启。
- 2026-05-29 代码复核发现 monitor 自动路由原先只抑制连续相同事件，交替的
  `tilt_left/tilt_right/gentle_shake/strong_shake` 仍可能按 `180ms` 周期高频切换
  表情。已在 `pet_motion.c` 增加 monitor 专用自动路由节流：
  `PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS=1500`，安全事件
  `PET_MOTION_MONITOR_ROUTE_SAFETY_COOLDOWN_MS=300`，统一由
  `pet_motion_monitor_route_allowed()` 判断；手动 `pet motion route <event>` 仍保留
  为不节流的诊断命令。
- 2026-05-29 节流版 OTA 产物归档在
  `output/2026-05-29-pet-motion-route-throttle/app_pack.rbl` 和 `all-app.bin`。
  `logs/serial/20260529-092442-ttyUSB0-115200.log` 显示
  `APP_OTA begin`、`cyg_recvlen_per:(100.00)%`、`write over` 和软件重启。
  `logs/serial/20260529-093008-ttyUSB0-115200.log` 再次确认 IMU 为 HXY 兼容器件、
  GPIO42/43、地址 `0x19`、`WHO_AM_I=0x11`、version `0x28`。
  `logs/serial/20260529-093055-ttyUSB0-115200.log` 显示 monitor 启动后空闲运行，
  `EYE_AVI_FPS avg=24.96-25.00`、`errors=0`、`overruns=0`，未出现自动
  `route motion=` 洪泛；`logs/serial/20260529-093417-ttyUSB0-115200.log`
  显示 `monitor disabled`，关断后继续 25fps。真实手持晃动/倾斜/冲击压力测试
  仍需人工配合后再关闭该风险。
- 2026-05-29 真实手持右侧垂直测试发现 `mg=(4,1007,332)` 一类轻微抖动样本会
  因 `gentle_shake` 判断早于 X/Y 倾斜而误判。已调整 `pet_motion_classify_sample()`
  顺序：`freefall/impact/strong_shake` 安全事件仍优先，随后先判定 X/Y dominant
  tilt，再进入 `gentle_shake`。复测日志
  `logs/serial/20260529-102602-ttyUSB0-115200.log` 显示
  `mg=(20,1019,318) ... event=tilt_right`，右侧垂直闭合。随后左侧垂直日志
  `logs/serial/20260529-131259-ttyUSB0-115200.log` 显示
  `mg=(41,-976,173) mag=1190 event=tilt_left`；放平日志
  `logs/serial/20260529-131358-ttyUSB0-115200.log` 显示
  `mg=(-24,63,-891) mag=978 event=stable`。至此右侧垂直、左侧垂直和放平三项
  静态姿态方向已闭合；自动姿态行为默认开启前仍需真实轻晃/剧烈晃动压力日志。
- 2026-05-29 真实 `caring.avi` 切换暴露 `/sf0` cache 失败后退回直接流式播放会
  把眼睛 FPS 拉到 `5-11fps`。已在 SDK `video_osi_wrapper.c` 增加
  cache-required 保护：manifest 已知 `/sf0/*.avi` 读取/hash 失败时拒绝本次打开，
  非 manifest 普通文件仍保留 POSIX fallback。新产物归档在
  `output/2026-05-29-avi-cache-required/`，`app_pack.rbl` sha256
  `5f5ddb47760766db65c41a8b9002e6f91a2904740a2cca146ed85d8330ef92a4`。
  `logs/serial/20260529-125130-ttyUSB0-115200.log` 显示
  `VIDEO_OSI avi cache required path=/sf0/caring.avi reason=cache required`，
  不再降级为流式播放；`logs/serial/20260529-125231-ttyUSB0-115200.log` 随后显示
  `caring.avi` 连续 `EYE_AVI_FPS avg=24.99-25.00`、`errors=0`、`overruns=0`。

### Phase 6: Pet Interaction Runtime

目标：把触摸、姿态、语音、云端、MCP、识图、隐私和安全事件统一为
`pet_behavior_request_t`，由 `pet_behavior_runtime` 仲裁眼神、提示音、震动、
云端 abort 和本地变量更新。

- 新增 `pet_prompt.[ch]`：提示音 ID manifest、路径生成和 TTS 互斥播放门控。
- 新增 `pet_behavior_runtime.[ch]`：输出优先级、节流和统一执行。
- `pet_action_router` 保留云端白名单解析，但不再直接操作 eye/haptic/abort。
- `pet_brain` 保持本地权威，离线事件表优先闭合。
- 本阶段不训练离线唤醒词，不默认开启业务振动硬件输出，不改 AVI/JPEG/display 热路径。
- `pet behavior status|event|json` 和 `pet prompt play <id>` 作为本阶段串口验收入口。

### Phase 7: Pet Interaction Scene Runtime

目标：在 `pet_brain` 前新增轻量 `pet_scene` 场景运行层，把真实触摸、语音、视觉、
姿态、隐私和错误入口统一成场景，补齐场景级互斥、隐私门控、视觉忙碌保护和串口
可观测状态。

- 新增 `pet_scene.[ch]`：维护 `current_scene`、`last_scene`、`last_event`、
  `voice_speaking`、`vision_busy`、`privacy_active` 和拒绝原因。
- 真实入口逐步从 `pet_brain_handle_event()` 前移到 `pet_scene_handle_event()`；
  `pet behavior event <event>` 保留为底层行为运行时诊断入口。
- 新增 `pet scene status|enter <scene>|event <event>` 作为场景层验收入口。
- 隐私态拒绝 `voice_listen` 和 `vision_capture`；视觉 busy 时拒绝重复
  `vision_capture`；普通重复场景有短冷却。
- 2026-05-30 追加在线/离线场景门控：`pet_scene` 记录 `cloud_online`，云端连接后
  才允许 `voice_listen` 和 `vision_capture` 这类依赖在线链路的场景；云端错误或断开
  后拒绝这些入口并返回 `offline`，避免离线时误播放“聆听/拍照”提示。
- `soft` 触觉 pattern 加入白名单，安抚场景不再 fallback 到 `off`；业务硬件输出
  仍由 `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 保护，默认不启用。
- 本阶段继续禁止修改 SDK AVI/JPEG/SPI/LVGL/display flush 热路径。

## AVI 回归护栏

后续每个阶段改动前后至少检查：

- 静态护栏 `test_pet_interaction_core_is_registered_without_touching_avi_hot_paths` 通过。
- `pet_action_router` 不包含 `bk_avi_player`、`bk_display_flush`、`bk_lcd_spi`、
  `jpeg_dec` 等显示/解码底层调用。
- 上板日志无 `EYE_FRAME_COLOR_ALERT`、`cmd NOT found: pet`、`Assert at`、
  `rtos_dump_system` 或重启。
- 表情切换日志仍为 `app_ui_display_chat_emotion` -> `EYE_SWITCH request/apply`。
- 常规播放段 `EYE_AVI_FPS` 平均不低于 15fps；当前已恢复到 manifest 校验后的
  `/sf0/*.avi` PSRAM cache 基线，2026-05-28 复测日志
  `logs/serial/20260528-194800-ttyUSB0-115200.log` 显示 `happy.avi` 连续
  `avg=24.97-25.00`、`errors=0`、`overruns=0`。manifest 已知 `/sf0/*.avi`
  cache 失败时必须拒绝打开，不能退回直接流式读取；非 manifest 普通文件才保留
  POSIX fallback。后续交互开发不能修改 SDK AVI/JPEG/SPI/LVGL/display flush
  热路径，也不能回退该 cache-required 保护。
- 2026-05-28 语音事件验证期间，快速切换 `curious`、`doubtful`、`smiling`、
  `confused` 后，最终 `confused.avi` 稳定段连续输出 `EYE_AVI_FPS avg=24.99-25.04`、
  `errors=0`、`overruns=0`，未见 `avi jpeg invalid`、`jpeg decode failed`、
  `EYE_FRAME_COLOR_ALERT`、`Assert at` 或 `HardFault`。若后续高频切换中看到
  `/sf0` cache retry/hash mismatch，但随后 manifest 校验命中并恢复 25fps，可按
  可恢复切换压力处理；不要在业务层做高频自动表情切换。
- 2026-05-29 IMU monitor 复测期间，`happy.avi` 在 monitor 开启约 70 秒内连续
  `EYE_AVI_FPS avg=24.98-25.00`、`errors=0`、`overruns=0`，并且关断后
  `pet motion ready=1 monitor=0`。该结果闭合 Phase 5 的低频 monitor 与 AVI
  cache 共存风险，但不等同于真实晃动/跌落压力测试通过。
- 2026-05-29 monitor 自动路由已加节流护栏，普通姿态事件最短间隔 `1500ms`，
  freefall/impact 安全事件最短间隔 `300ms`；任何后续默认开启自动姿态行为的改动，
  必须保留这类节流并补真实晃动压力日志，确认 `route motion=` 不会按采样周期刷屏。
