# 宠物交互场景运行时设计

日期：2026-05-30

## 目标

在已经跑通的 `pet_brain -> pet_behavior_runtime -> pet_prompt/pet_haptic/eye`
基础上，新增一层轻量的交互场景运行时 `pet_scene`。它负责把真实入口和串口诊断
入口统一成“场景”，并在进入 `pet_brain` 前做场景级互斥、隐私门控、忙碌保护和
可观测状态记录。

第一版目标不是重写宠物大脑，也不是做复杂人格系统；目标是让触摸、语音、拍照、
姿态、隐私和错误降级这些用户能感知的交互，按统一场景规则闭环、可测、可继续
调音频和动作。

## 当前基线

- `pet_brain` 已有本地事件到行为包的映射，覆盖触摸、语音、云端、视觉、隐私、
  睡眠和姿态事件。
- `pet_behavior_runtime` 已统一执行眼神、提示音和触觉，并有 TTS 互斥和提示音
  冷却。
- `pet_prompt` 已使用 `/if0/prompts/<prompt_id>.mp3` 扁平路径，已补齐
  `thinking`、`processing`、`success`、`fail`、`sleep` 等资源 ID。
- 上板已验证连续提示音坏帧问题可通过行为层 `2500ms` prompt cooldown 缓解。
- `soft` 触觉模式目前仍未在 `pet_haptic` 白名单中，业务调用会 fallback 到 `off`。

## 非目标

- 不修改 SDK AVI、JPEG、SPI、LVGL、display flush 或播放器底层热路径。
- 不训练或接入新的离线唤醒词模型。
- 不默认开启 MS32008N1 OUT5 业务硬件震动输出。
- 不做长期性格、记忆或复杂行为树。
- 不把拍照、上传、TTS、文件播放这类耗时工作放到 ISR、audio、display 或 protocol
  热回调里。

## 架构

```text
touch / motion / voice / camera / cloud / CLI
        |
        v
pet_scene
        |  场景互斥、隐私门控、忙碌状态、日志
        v
pet_brain
        |  本地状态和行为包生成
        v
pet_behavior_runtime
        |  eye / prompt / haptic / abort 仲裁执行
        v
app_ui / dialog prompt / pet_haptic / protocol
```

`pet_scene` 只决定“现在是否应该进入这个场景，以及这个场景对应哪个 pet event”。
它不直接操作眼睛、提示音、震动、摄像头、WebSocket 或显示底层。

## 场景集合

第一版支持以下场景：

| 场景 | 入口 | 映射事件 | 说明 |
| --- | --- | --- | --- |
| `touch_play` | 摸头短触、轻互动 | `touch_head_short` | 开心短反馈 |
| `soothe` | 摸头长按、安抚 | `touch_head_long` | caring 眼神、安抚音、soft 触觉 |
| `curious` | 下巴短触、倾斜 | `touch_chin_short` 或 motion tilt | 好奇反馈 |
| `voice_listen` | 开始聆听 | `audio_listen_start` | 进入云端聆听 |
| `voice_think` | ASR 结束/等待回复 | `audio_listen_stop` | 进入思考态 |
| `voice_speak` | TTS 开始 | `audio_tts_start` | 云端 TTS 独占扬声器 |
| `voice_done` | TTS 结束 | `audio_tts_stop` | 回到 idle |
| `vision_capture` | 拍照/识图开始 | `vision_capture_start` | 拍照提示与 photo 眼神 |
| `vision_done` | 拍照识图完成 | `vision_capture_done` | 成功反馈 |
| `vision_error` | 拍照识图失败 | `vision_capture_error` | 错误降级 |
| `privacy_on` | 隐私开启 | `privacy_on` | 禁止聆听和拍照误提示 |
| `privacy_off` | 隐私关闭 | `privacy_off` | 回到可互动 |
| `error_net` | 云端/网络错误 | `cloud_error` | 本地降级 |
| `motion_gentle` | 轻晃 | `motion_gentle_shake` | 开心反馈 |
| `motion_alert` | 重晃、跌落、冲击 | `motion_strong_shake/freefall/impact` | 安全优先 |
| `idle` | 手动回 idle | `idle` | 收敛状态 |

## 场景互斥规则

第一版只做明确、低风险的互斥：

- `privacy` 激活时，拒绝 `voice_listen` 和 `vision_capture`，但允许触摸、隐私关闭、
  安全事件和 idle。
- `voice_speak` 激活时，普通本地场景可以更新眼神但普通提示音由
  `pet_behavior_runtime` 跳过；安全、隐私、停止类场景仍可打断。
- `vision_capture` 激活时，重复 `vision_capture` 直接跳过，避免重复拍照和重复
  提示音。
- `motion_alert`、`privacy_on`、`privacy_off`、`idle` 是高优先级场景，可以打断
  当前普通场景。
- 同一普通场景短时间重复进入需要节流，第一版使用 `PET_SCENE_REPEAT_COOLDOWN_MS`
  保护串口和真实入口。

## 状态记录

`pet_scene` 维护轻量状态，供串口诊断和日志判断：

```c
typedef struct {
    pet_scene_type_t current_scene;
    pet_scene_type_t last_scene;
    pet_event_type_t last_event;
    uint32_t last_enter_ms;
    uint32_t rejected_count;
    bool voice_speaking;
    bool vision_busy;
    bool privacy_active;
} pet_scene_status_t;
```

日志必须能回答三个问题：

```text
pet_scene enter scene=vision_capture event=vision_capture_start
pet_scene reject scene=vision_capture reason=vision_busy
pet scene current=vision_capture last=touch_play event=vision_capture_start ...
```

## 触觉补齐

第一版补齐 `soft` 触觉白名单：

| Pattern | 默认时长 | 用途 |
| --- | --- | --- |
| `soft` | 320ms | 安抚、长按头部 |

硬件输出仍由 `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 保护，默认保持关闭。补齐白名单
只解决业务语义 fallback 到 `off` 的问题。

## CLI 验收入口

新增：

```text
pet scene status
pet scene enter <scene>
pet scene event <event>
```

`pet scene enter <scene>` 用于按产品场景测试；`pet scene event <event>` 用于验证
真实事件经过场景层后的门控行为。旧的 `pet behavior event <event>` 保留，用于
直接测试 `pet_brain` 和行为运行时，不走场景层。

## 验证策略

主机检查：

- 静态护栏证明 `pet_scene.[ch]` 已加入 CMake。
- 静态护栏证明真实入口逐步调用 `pet_scene_handle_event()`，而不是继续全部直连
  `pet_brain_handle_event()`。
- 静态护栏禁止 `pet_scene` 触碰 AVI、JPEG、SPI、LVGL、display flush、camera
  上传和播放器底层接口。
- 静态护栏证明 `soft` 触觉 pattern 存在。

上板检查：

```text
pet scene status
pet scene enter touch_play
pet scene enter soothe
pet scene enter voice_speak
pet scene enter touch_play
pet scene enter voice_done
pet scene enter privacy_on
pet scene enter vision_capture
pet scene enter privacy_off
pet scene enter vision_capture
pet scene enter vision_done
pet scene status
pet haptic soft
pet haptic status
```

预期：

- 场景日志清晰显示 enter/reject。
- 隐私态拒绝 `vision_capture`。
- TTS 态普通提示音不会打断云端扬声器。
- `soft` 不再 fallback 到 `off`。
- 无 `ERR_MP3_INVALID_FRAMEHEADER`、Assert、HardFault、rtos dump。
- `EYE_AVI_FPS` 仍维持约 25fps。

