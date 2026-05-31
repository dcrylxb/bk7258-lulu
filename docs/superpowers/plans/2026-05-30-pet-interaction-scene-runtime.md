# 宠物交互场景运行时实现计划

> **给执行 agent：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 按任务逐步执行本计划。步骤使用 checkbox（`- [ ]`）跟踪。

**目标：** 新增 `pet_scene` 场景运行层，把触摸、语音、视觉、姿态、隐私和错误降级入口统一到可观测、可互斥、可测试的场景闭环。

**架构：** `pet_scene` 是 `pet_brain` 前的一层轻量门控。它维护当前场景、隐私、TTS、视觉忙碌和拒绝计数，真实入口和 CLI 入口调用 `pet_scene_handle_event()`；通过门控后继续委托 `pet_brain_handle_event()` 生成行为包，由既有 `pet_behavior_runtime` 执行。

**技术栈：** BK7258 Armino SMP C firmware、现有 `pet_brain`、`pet_behavior_runtime`、`pet_prompt`、`pet_haptic`、Python 静态护栏、串口 CLI 验证。

---

## 文件地图

新增：

- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_scene.h`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_scene.c`

修改：

- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/system_manager/system_manager.c`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/key/app_key.c`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/iot/iot_camera.c`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_motion.c`
- `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_haptic.c`
- `tools/tests/test_cmaiw82al_ai_toy_static_guards.py`
- `cmaiw82al_ai_toy/docs/08_ai_pet_interaction_core_plan.md`

不要修改：

- SDK AVI/JPEG/SPI/LVGL/display flush 热路径。
- `/sf0` 眼神资源和 manifest cache-required 行为。
- `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 默认值。

---

## Task 1: 场景运行层静态护栏

**文件：**

- 修改：`tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

步骤：

- [ ] 增加测试：`pet_scene.[ch]` 必须注册到 CMake，并暴露 `pet_scene_handle_event`、`pet_scene_enter`、`pet_scene_get_status`、`pet_scene_type_name`。
- [ ] 增加测试：`pet_scene.c` 不能包含 `bk_avi_player`、`bk_display_flush`、`bk_lcd_spi`、`jpeg_dec`、`bk_jpeg`、`lv_disp_flush`、`iot_camera_take_photo_and_explain`、`speaker_play_prompt_tone`。
- [ ] 增加测试：CLI 必须包含 `pet scene status`、`pet scene enter <scene>`、`pet scene event <event>`。
- [ ] 增加测试：`system_manager.c`、`app_key.c`、`iot_camera.c`、`pet_motion.c` 必须调用 `pet_scene_handle_event(...)`。
- [ ] 增加测试：`pet_haptic.c` 必须包含 `{"soft", 320}`，保证安抚场景不再 fallback 到 `off`。
- [ ] 运行 `python3 -m pytest -q tools/tests/test_cmaiw82al_ai_toy_static_guards.py`，确认新增护栏先失败。

## Task 2: 实现 `pet_scene`

**文件：**

- 新增：`ap/main/pet/pet_scene.h`
- 新增：`ap/main/pet/pet_scene.c`
- 修改：`ap/CMakeLists.txt`

步骤：

- [ ] 定义 `pet_scene_type_t`、`pet_scene_reject_reason_t`、`pet_scene_status_t`。
- [ ] 实现 `pet_scene_event_to_scene()`，把 `pet_event_type_t` 映射为场景。
- [ ] 实现隐私门控：隐私态拒绝 `PET_SCENE_VOICE_LISTEN` 和 `PET_SCENE_VISION_CAPTURE`。
- [ ] 实现视觉忙碌门控：`PET_SCENE_VISION_CAPTURE` 进入后，重复 capture 被拒绝，直到 `vision_done` 或 `vision_error`。
- [ ] 实现 TTS 状态记录：`voice_speak` 设置 `voice_speaking=true`，`voice_done` 清除。
- [ ] 实现重复场景冷却：普通场景短时间重复进入时拒绝，高优先级场景不受普通冷却限制。
- [ ] 通过门控后调用 `pet_brain_handle_event(event)`。

## Task 3: 接入真实入口和 CLI

**文件：**

- 修改：`ap/main/cli/cli_app_pet.c`
- 修改：`ap/main/system_manager/system_manager.c`
- 修改：`ap/main/key/app_key.c`
- 修改：`ap/main/iot/iot_camera.c`
- 修改：`ap/main/pet/pet_motion.c`

步骤：

- [ ] CLI 引入 `pet_scene.h`，新增 `pet scene status`。
- [ ] CLI 新增 `pet scene enter <scene>`，按场景名调用 `pet_scene_enter()`。
- [ ] CLI 新增 `pet scene event <event>`，按事件名调用 `pet_scene_handle_event()`。
- [ ] 触摸入口从 `pet_brain_handle_event()` 改为 `pet_scene_handle_event()`。
- [ ] system manager 语音/网络事件从 `pet_brain_handle_event()` 改为 `pet_scene_handle_event()`。
- [ ] camera vision start/done/error 从 `pet_brain_handle_event()` 改为 `pet_scene_handle_event()`。
- [ ] motion 自动路由从 `pet_brain_handle_event()` 改为 `pet_scene_handle_event()`。

## Task 4: 补齐 soft 触觉 pattern

**文件：**

- 修改：`ap/main/pet/pet_haptic.c`

步骤：

- [ ] 在 `s_haptic_patterns` 增加 `{"soft", 320}`。
- [ ] 保持 `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 默认关闭。
- [ ] 确认 `pet_haptic_play("soft", 0)` 会记录 `soft` 和默认时长，不再 fallback 到 `off`。

## Task 5: 文档和验证

**文件：**

- 修改：`docs/08_ai_pet_interaction_core_plan.md`

步骤：

- [ ] 增加 Phase 7 交互场景运行时说明。
- [ ] 运行静态护栏：`python3 -m pytest -q tools/tests/test_cmaiw82al_ai_toy_static_guards.py`。
- [ ] 构建固件：`cd cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy && SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258`。
- [ ] 若构建通过，归档产物到 `output/2026-05-30-pet-scene-runtime/`。
- [ ] 上板验证前先确认 `/dev/ttyUSB0` 空闲；本轮如用户未要求刷机，可以先停在构建产物。

