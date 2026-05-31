# 宠物交互运行时设计

日期：2026-05-30

## 目标

为 BK7258 AI 电子宠物建立一套统一的宠物交互运行时。运行时负责把触摸、姿态、
语音、云端、MCP、视觉、隐私和安全事件统一转换成受限的行为包，再由同一个
仲裁层执行眼神、提示音、震动、云端 abort 和本地状态更新。

目标体验是本地优先：离线时宠物仍然能即时响应；联网 AI 只能增强行为，不能
接管宠物身体。

## 当前基线

当前板子已经验证的能力如下：

- 双 GC9D01 眼睛在 `/sf0/*.avi` 资源命中 cache 且满足 `320x160` AVI 合同
  时，可以稳定播放到约 25 fps。
- Xiaozhi 在线语音对话链路已经通过 WebSocket 跑通。
- 摄像头 MCP 拍照识图已经跑通：GC2145 抓拍后 fallback 上传到
  `http://106.55.173.79:8989/xiaozhi/api/vision`。
- IMU 已验证平放、左侧垂直、右侧垂直方向，monitor 自动路由已有节流。
- MS32008N1 OUT5 可以通过显式诊断命令驱动 CN3 2pin 直流振动马达，但业务
  haptic 输出仍处于保护关闭状态。
- 官方 `hello`/`byebye` 离线 KWS 在当前板子和阈值下不够稳定，不作为本阶段
  产品行为入口。自训练离线唤醒词不属于本运行时阶段目标。

现有固件已经有 `pet_brain`、`pet_action_router`、`pet_haptic`、
`pet_motion`、`pet_vision`、`iot_pet`、`iot_camera` 和串口 `pet` 命令。
目前缺口是行为执行还分散在 brain 映射、router 执行、system event 和临时
prompt 调用里，没有统一仲裁。

## 非目标

- 本阶段不训练自定义离线唤醒词模型。
- 不修改 SDK AVI、JPEG、SPI、LVGL 或 display flush 热路径。
- 在后续供电余量和屏幕闪烁调优前，不默认开启业务 haptic 硬件输出。
- 不向云端工具暴露原始 GPIO、PWM、flash、camera、display 或 memory 控制。
- 不在 protocol、audio、display、ISR 或 IPC 回调内执行长耗时 camera、
  network、file 或 audio 操作。

## 架构

```text
touch / motion / voice / cloud / MCP / vision / privacy / safety
        |
        v
pet_event
        |
        v
pet_brain              local authority for state, variables, and policy
        |
        v
pet_behavior_runtime   behavior package arbitration and execution
        |
        +--> eye runtime through app_ui_display_chat_emotion()
        +--> prompt runtime through dialog/audio prompt API
        +--> haptic runtime through pet_haptic pattern API
        +--> cloud abort through protocol abstraction
        +--> local variable delta through pet_brain
```

`pet_brain` 决定应该发生什么；`pet_behavior_runtime` 决定当前是否允许发生，
并按正确顺序执行。`pet_action_router` 保留云端 JSON 白名单和兼容适配职责，
但不再直接操作眼神、提示音、震动或云端 abort，而是委托给运行时。

## 行为包合同

每个本地事件和云端 `pet_action` 都会规范化为以下语义结构：

```c
typedef enum {
    PET_BEHAVIOR_SOURCE_LOCAL,
    PET_BEHAVIOR_SOURCE_CLOUD,
    PET_BEHAVIOR_SOURCE_MCP,
    PET_BEHAVIOR_SOURCE_CLI,
} pet_behavior_source_t;

typedef enum {
    PET_BEHAVIOR_SAFETY_NORMAL,
    PET_BEHAVIOR_SAFETY_USER_FEEDBACK,
    PET_BEHAVIOR_SAFETY_SAFETY,
    PET_BEHAVIOR_SAFETY_PRIVACY,
} pet_behavior_safety_t;

typedef struct {
    pet_action_type_t action;
    const char *emotion;
    const char *prompt_id;
    const char *haptic;
    uint32_t duration_ms;
    pet_local_delta_t local_delta;
    pet_behavior_source_t source;
    pet_behavior_safety_t safety;
    bool send_cloud_abort;
    bool can_interrupt_tts;
    bool requires_online;
} pet_behavior_request_t;
```

云端 JSON 只允许提交白名单内的 `action`、`emotion`、`haptic`、`prompt_id`、
`duration_ms` 和 `local_delta`。不支持的字段忽略。云端 `local_delta` 沿用现有
`[-5, 5]` 裁剪限制；本地安全事件如果需要更强 delta，只能通过本地显式规则表
产生。

## 输出仲裁

运行时使用固定输出优先级：

```text
privacy / stop / impact / freefall
  > active cloud TTS
  > explicit local user feedback
  > connect / error / vision / state prompts
  > idle ambient behavior
```

规则：

- 安全和隐私动作可以打断提示音，并且可以发送 cloud abort。
- 云端 TTS 播放期间独占扬声器。本地短提示音必须跳过或延后，除非行为显式设置
  `can_interrupt_tts=true`。
- 本地提示音可在 idle、离线反应、连接/错误反馈和人工测试提示场景播放。
- 眼神只通过语义 emotion 请求执行。运行时禁止调用 AVI 或 display 底层接口。
- 震动只通过 `pet_haptic_play()` 执行。即使业务硬件输出仍关闭，运行时也要能
  接受并记录 haptic 请求。
- 非安全类重复 eye/prompt/haptic 动作必须节流，避免 AVI 高频切换、提示音刷屏
  和电机功耗尖峰。

## 提示音设计

提示音文件位于内部 `/if0` 资源区。产品层通过稳定的 prompt ID manifest 引用，
避免在业务代码中到处写文件名。

运行时路径约定：

```text
/if0/prompts/<prompt_id>.mp3
```

实测注意：`/if0/prompts/power_on_16k_mono_16bit_zh.mp3` 这类一层 prompt
路径可稳定 `stat()`、打开和播放；`/if0/prompts/pet/<prompt_id>.mp3`
在当前板端 VFS 上会出现 `stat()` 失败。因此运行时使用扁平路径，
打包脚本仍保留 `prompts/pet/` 源目录并自动同步一份到 `prompts/` 根下。

推荐资源格式：

- 使用现有 prompt/player 路径支持的 MP3 或 WAV。
- 单声道。
- 优先 16 kHz。
- 交互提示音长度控制在 200 ms 到 1200 ms。
- 峰值音量保守归一化，因为当前扬声器大声播放时可能引起可见电源压降。

首批 prompt ID：

| 分类 | Prompt ID |
| --- | --- |
| 系统 | `boot`, `net_ok`, `net_lost`, `low_power`, `error` |
| 交互 | `listen_start`, `cancel`, `photo`, `done` |
| 宠物反应 | `happy_chirp`, `curious`, `afraid`, `comforted` |
| 安全/隐私 | `impact`, `privacy_on`, `privacy_off`, `stop` |
| 测试/操作员 | `test_wake_prompt` |

提示音播放必须有可观测日志：

```text
pet_prompt play id=listen_start path=/if0/prompts/listen_start.mp3
pet_prompt skip id=happy_chirp reason=tts_active
pet_prompt miss id=net_ok path=/if0/prompts/net_ok.mp3
```

## 离线响应机制

离线包括无可用云端 session、云端错误/降级、隐私模式或明确本地-only 交互。
离线行为仍必须即时更新宠物身体反馈：

| 事件 | 状态 | 表情 | 提示音 | 震动 | 本地变量变化 |
| --- | --- | --- | --- | --- | --- |
| head short | `LOCAL_REACT` | `happy` | `happy_chirp` | `tap` | mood +1, affection +1 |
| head long | `SOOTHE` | `caring` | `comforted` | `soft` | stress -5, affection +2 |
| chin short, offline | `LOCAL_REACT` | `curious` | `curious` | none | novelty +1 |
| chin long | `IDLE_ALIVE` | `confused` | `cancel` | `tap` | none, cloud abort if active |
| chin very long | `PRIVACY` | `doubtful` | `privacy_on` | `confirm` | privacy on |
| gentle shake | `LOCAL_REACT` | `happy` | `happy_chirp` | `confirm` | mood +1, novelty +2 |
| strong shake | `SOOTHE` | `grimacing` | `impact` | `alert` | stress +3 |
| freefall/impact | `SOOTHE` | `afraid` | `impact` | `alert` | stress +5, security -5 |
| wake from sleep | `IDLE_ALIVE` | `happy` | `boot` | `tap` | energy -1 |

这张表是第一版行为库。实现时应作为 `pet_brain` 内的数据化映射或小型本地规则表，
不要散落在各类回调函数里。

## 在线交互桥

在线状态变化也统一转换为行为包：

| 来源事件 | 行为 |
| --- | --- |
| cloud connected | `happy` + `net_ok` prompt if not speaking |
| listen start | `curious` + `listen_start` prompt if prompt is allowed |
| listen stop / ASR done | `doubtful`, no prompt |
| TTS start | `CLOUD_SPEAK`, eye only, no local prompt |
| TTS stop | return to `IDLE_ALIVE` with emotion selected by last cloud action or `smiling` |
| cloud error | `confused` + `net_lost` or `error` prompt |
| vision capture start | `photo` + `photo` prompt if not speaking |
| vision result spoken | cloud TTS owns speaker; no local prompt |
| cloud `pet_action` | whitelist, clip, then runtime arbitration |

桥接层必须使用现有 `SYSTEM_EVENT_*`、`protocol_instance()` 和 MCP 边界。protocol
接收回调可以解析并投递事件，或调用有界适配接口，但不能在线程回调里直接执行长
耗时 camera、prompt 或行为序列。

## 模块边界

计划新增或调整的产品模块：

| 模块 | 职责 |
| --- | --- |
| `pet_behavior_runtime.[ch]` | 执行行为包，负责优先级、节流和输出顺序。 |
| `pet_prompt.[ch]` | prompt ID manifest、路径生成、文件存在检查和播放门控。 |
| `pet_brain.[ch]` | 本地状态、变量、隐私、离线事件策略和行为请求生成。 |
| `pet_action_router.[ch]` | 解析和规范化云端/CLI action JSON，再委托运行时执行。 |
| `cli_app_pet.c` | prompt、behavior、event、status、voice、motion、haptic、vision 诊断入口。 |
| `system_manager.c` | 把 voice/network 生命周期事件发布给 `pet_brain`，避免直接执行输出。 |
| `iot_pet.c` | 只暴露语义化宠物工具，不暴露原始硬件控制。 |

## 与现有代码的集成方式

实现应按渐进路径推进：

1. 先加静态护栏和文档。
2. 新增 `pet_prompt`，提供保守的播放门控和 CLI 验证。
3. 新增 `pet_behavior_runtime`，先包装当前 `pet_action_router` 已经使用的 eye、
   haptic、prompt 和 abort API。
4. 调整 `pet_brain` 生成 `pet_behavior_request_t`。
5. 调整 `pet_action_router_execute()` 委托给 `pet_behavior_runtime_execute()`。
6. 本地运行时行为可观测后，再加入在线连接、聆听、错误和视觉事件提示。

这条路径可以保证迁移期间现有语音、摄像头、IMU、haptic 诊断和眼神输出仍可用。

## 验证策略

主机/静态检查：

- 静态护栏必须证明新模块已经注册。
- 静态护栏必须禁止 behavior runtime/router 调用 AVI、JPEG、SPI、LVGL 或
  display flush 底层接口。
- 静态护栏必须证明本地提示音受在线 TTS 状态门控。
- 固件构建必须通过。

上板检查：

- 打开串口日志前，先确认没有其他进程占用 `/dev/ttyUSB0`。
- OTA 新固件并确认启动日志。
- 串口执行：

```text
pet prompt play test_wake_prompt
pet behavior event touch_head_short
pet behavior event touch_head_long
pet behavior event motion_impact
pet behavior json {"action":"emote","emotion":"happy","prompt_id":"happy_chirp","haptic":"tap","duration_ms":800}
pet status
pet voice status
```

期望日志：

- `pet_behavior accept` / `pet_behavior skip` 和 reason。
- `pet_prompt play` / `skip` / `miss`。
- `pet haptic` accepted 或 skipped，不能意外打开硬件输出。
- `EYE_SWITCH` 仍通过现有 UI API。
- `EYE_AVI_FPS` 保持稳定，不出现 `Assert at`、`HardFault`、
  `rtos_dump_system`、`EYE_FRAME_COLOR_ALERT` 或黄蓝 fallback。

人工配合测试：

- 需要用户说话或操作板子时，如果音频可用，先播放 `test_wake_prompt` 提醒用户。
- 抓日志窗口时要覆盖用户动作前后；用户回复可能滞后 10 到 15 秒，所以要保留更早
  的串口历史。

## 推进阶段

Phase A：设计/runtime skeleton 和静态护栏。

Phase B：prompt manifest 和 CLI 播放门控。

Phase C：通过 behavior runtime 跑通本地离线行为表。

Phase D：cloud action router 委托和 MCP 兼容。

Phase E：连接、聆听、错误和视觉的在线生命周期提示。

Phase F：上板验证、日志证据，并更新 `docs/08_ai_pet_interaction_core_plan.md`
和 prompt 资源文档。

这些阶段完成后，再回到供电余量、屏幕闪烁、扬声器响度和默认业务 haptic 开启的
整机调优。

## 自检

- 不包含自定义 KWS 训练。
- 明确保护 display 和 audio 热路径。
- 云端控制仍受本地白名单和仲裁限制。
- 离线行为足够具体，可以直接拆实现。
- 提示音包含路径、格式、分类和日志合同。
