# 宠物交互运行时实现计划

> **给执行 agent：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 按任务逐步执行本计划。步骤使用 checkbox（`- [ ]`）跟踪。

**目标：** 新增统一的宠物行为运行时，让本地/离线事件和受限云端动作都通过同一个仲裁层执行 eye、prompt、haptic、abort 和状态反馈。

**架构：** `pet_brain` 继续作为本地权威，负责生成语义行为请求。新增 `pet_prompt` 管理 prompt ID、文件路径和扬声器门控；新增 `pet_behavior_runtime` 管理输出优先级、节流和执行。现有 `pet_action_router` 保留 cloud/CLI JSON 白名单解析，然后委托给运行时执行。

**技术栈：** BK7258 Armino SMP C firmware、现有 `pet_*` 模块、现有 `dialog_module` prompt API、现有 `app_ui_display_chat_emotion()`、现有 `pet_haptic`、cJSON、Python 静态护栏测试、串口 CLI 验证。

---

## 范围和文件地图

Spec: `/home/jason/armino1/cmaiw82al_ai_toy/docs/superpowers/specs/2026-05-30-pet-interaction-runtime-design.md`

新增：

- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_prompt.h`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_prompt.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_behavior_runtime.h`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_behavior_runtime.c`

修改：

- `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.h`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.h`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/system_manager/system_manager.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/iot/iot_pet.c`
- `/home/jason/armino1/cmaiw82al_ai_toy/resources/prompts/README.md`
- `/home/jason/armino1/cmaiw82al_ai_toy/docs/08_ai_pet_interaction_core_plan.md`

不要修改：

- SDK AVI/JPEG/SPI/LVGL/display 热路径。
- `/sf0` AVI 资源格式或 manifest cache-required 行为。
- 默认业务 haptic 硬件输出开关。

`/home/jason/armino1` 根目录不是 git 仓库。除非后续在真实 worktree 中执行，否则不要加入 commit 步骤。

---

### Task 1: 运行时边界静态护栏

**文件：**
- 修改： `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

- [ ] **Step 1: 增加新模块注册的失败优先护栏**

把这个测试方法加入 `StaticGuardTests`：

```python
    def test_pet_interaction_runtime_modules_are_registered(self):
        cmake = read("ap/CMakeLists.txt")
        pet_prompt_h = read_optional("ap/main/pet/pet_prompt.h")
        pet_prompt_c = read_optional("ap/main/pet/pet_prompt.c")
        pet_runtime_h = read_optional("ap/main/pet/pet_behavior_runtime.h")
        pet_runtime_c = read_optional("ap/main/pet/pet_behavior_runtime.c")
        pet_cli = read("ap/main/cli/cli_app_pet.c")

        for marker in [
            "./main/pet/pet_prompt.c",
            "./main/pet/pet_behavior_runtime.c",
        ]:
            self.assertIn(marker, cmake)

        for marker in [
            "PET_PROMPT_ID_LISTEN_START",
            "PET_PROMPT_BASE_PATH",
            "pet_prompt_play",
            "pet_prompt_path_for_id",
        ]:
            self.assertIn(marker, pet_prompt_h + pet_prompt_c)

        for marker in [
            "pet_behavior_request_t",
            "pet_behavior_runtime_execute",
            "pet_behavior_runtime_set_cloud_tts_active",
            "pet_behavior_runtime_get_status",
        ]:
            self.assertIn(marker, pet_runtime_h + pet_runtime_c)

        for marker in [
            "pet prompt play <id>",
            "pet behavior event <event>",
            "pet behavior json <json>",
        ]:
            self.assertIn(marker, pet_cli)
```

- [ ] **Step 2: 增加 display 热路径保护的失败优先护栏**

加入这个测试方法：

```python
    def test_pet_behavior_runtime_does_not_touch_display_hot_paths(self):
        runtime = read_optional("ap/main/pet/pet_behavior_runtime.c")
        router = read("ap/main/pet/pet_action_router.c")

        forbidden = [
            "bk_avi_player",
            "bk_display_flush",
            "bk_lcd_spi",
            "jpeg_dec",
            "bk_jpeg",
            "lv_disp_flush",
        ]

        for marker in forbidden:
            self.assertNotIn(marker, runtime)
            self.assertNotIn(marker, router)

        self.assertIn("app_ui_display_chat_emotion", runtime)
```

- [ ] **Step 3: 增加 prompt/TTS 仲裁的失败优先护栏**

加入这个测试方法：

```python
    def test_pet_prompt_is_gated_against_cloud_tts(self):
        prompt = read_optional("ap/main/pet/pet_prompt.c")
        runtime = read_optional("ap/main/pet/pet_behavior_runtime.c")
        brain = read("ap/main/pet/pet_brain.c")

        self.assertIn("PET_PROMPT_SKIP_TTS_ACTIVE", prompt + runtime)
        self.assertIn("can_interrupt_tts", runtime)
        self.assertIn("tts_active", runtime)
        self.assertIn("PET_EVENT_AUDIO_TTS_START", brain)
        self.assertIn("pet_behavior_runtime_set_cloud_tts_active(true)", brain)
        self.assertIn("PET_EVENT_AUDIO_TTS_STOP", brain)
        self.assertIn("pet_behavior_runtime_set_cloud_tts_active(false)", brain)
```

- [ ] **Step 4: 运行护栏，并确认实现前按预期失败**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

期望：FAIL，失败原因指向缺少 `pet_prompt`、`pet_behavior_runtime` 和 CLI 标记。

---

### Task 2: Prompt Manifest 和播放门控

**文件：**
- 新增： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_prompt.h`
- 新增： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_prompt.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`

- [ ] **Step 1: 创建 `pet_prompt.h`**

```c
#ifndef __PET_PROMPT_H__
#define __PET_PROMPT_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PET_PROMPT_BASE_PATH "/if0/prompts/pet"
#define PET_PROMPT_PATH_MAX  96

typedef enum {
    PET_PROMPT_RESULT_PLAYED = 0,
    PET_PROMPT_RESULT_SKIPPED,
    PET_PROMPT_RESULT_MISSING,
    PET_PROMPT_RESULT_ERROR,
} pet_prompt_result_t;

typedef enum {
    PET_PROMPT_SKIP_NONE = 0,
    PET_PROMPT_SKIP_EMPTY_ID,
    PET_PROMPT_SKIP_TTS_ACTIVE,
    PET_PROMPT_SKIP_NOT_ALLOWED,
} pet_prompt_skip_reason_t;

typedef struct {
    bool tts_active;
    bool can_interrupt_tts;
} pet_prompt_gate_t;

const char *PET_PROMPT_ID_BOOT(void);
const char *PET_PROMPT_ID_NET_OK(void);
const char *PET_PROMPT_ID_NET_LOST(void);
const char *PET_PROMPT_ID_ERROR(void);
const char *PET_PROMPT_ID_LISTEN_START(void);
const char *PET_PROMPT_ID_CANCEL(void);
const char *PET_PROMPT_ID_PHOTO(void);
const char *PET_PROMPT_ID_HAPPY_CHIRP(void);
const char *PET_PROMPT_ID_CURIOUS(void);
const char *PET_PROMPT_ID_IMPACT(void);
const char *PET_PROMPT_ID_PRIVACY_ON(void);
const char *PET_PROMPT_ID_PRIVACY_OFF(void);
const char *PET_PROMPT_ID_STOP(void);
const char *PET_PROMPT_ID_TEST_WAKE_PROMPT(void);

bool pet_prompt_id_is_allowed(const char *prompt_id);
bk_err_t pet_prompt_path_for_id(const char *prompt_id, char *path, uint32_t path_len);
pet_prompt_result_t pet_prompt_play(const char *prompt_id,
                                    const pet_prompt_gate_t *gate,
                                    pet_prompt_skip_reason_t *skip_reason);
const char *pet_prompt_result_name(pet_prompt_result_t result);
const char *pet_prompt_skip_reason_name(pet_prompt_skip_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: 创建 `pet_prompt.c`**

```c
#include <stdio.h>
#include <os/str.h>
#include <components/log.h>

#include "dialog_module.h"
#include "pet_prompt.h"

#define TAG "pet_prompt"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    const char *id;
} pet_prompt_entry_t;

const char *PET_PROMPT_ID_BOOT(void) { return "boot"; }
const char *PET_PROMPT_ID_NET_OK(void) { return "net_ok"; }
const char *PET_PROMPT_ID_NET_LOST(void) { return "net_lost"; }
const char *PET_PROMPT_ID_ERROR(void) { return "error"; }
const char *PET_PROMPT_ID_LISTEN_START(void) { return "listen_start"; }
const char *PET_PROMPT_ID_CANCEL(void) { return "cancel"; }
const char *PET_PROMPT_ID_PHOTO(void) { return "photo"; }
const char *PET_PROMPT_ID_HAPPY_CHIRP(void) { return "happy_chirp"; }
const char *PET_PROMPT_ID_CURIOUS(void) { return "curious"; }
const char *PET_PROMPT_ID_IMPACT(void) { return "impact"; }
const char *PET_PROMPT_ID_PRIVACY_ON(void) { return "privacy_on"; }
const char *PET_PROMPT_ID_PRIVACY_OFF(void) { return "privacy_off"; }
const char *PET_PROMPT_ID_STOP(void) { return "stop"; }
const char *PET_PROMPT_ID_TEST_WAKE_PROMPT(void) { return "test_wake_prompt"; }

static const pet_prompt_entry_t s_prompt_manifest[] = {
    {"boot"},
    {"net_ok"},
    {"net_lost"},
    {"low_power"},
    {"error"},
    {"listen_start"},
    {"cancel"},
    {"photo"},
    {"done"},
    {"happy_chirp"},
    {"curious"},
    {"afraid"},
    {"comforted"},
    {"impact"},
    {"privacy_on"},
    {"privacy_off"},
    {"stop"},
    {"test_wake_prompt"},
};

bool pet_prompt_id_is_allowed(const char *prompt_id)
{
    if (prompt_id == NULL || prompt_id[0] == '\0') {
        return false;
    }

    for (uint32_t i = 0; i < sizeof(s_prompt_manifest) / sizeof(s_prompt_manifest[0]); i++) {
        if (os_strcmp(prompt_id, s_prompt_manifest[i].id) == 0) {
            return true;
        }
    }

    return false;
}

bk_err_t pet_prompt_path_for_id(const char *prompt_id, char *path, uint32_t path_len)
{
    int written = 0;

    if (path == NULL || path_len == 0 || !pet_prompt_id_is_allowed(prompt_id)) {
        return BK_ERR_PARAM;
    }

    written = snprintf(path, path_len, "%s/%s.mp3", PET_PROMPT_BASE_PATH, prompt_id);
    if (written <= 0 || (uint32_t)written >= path_len) {
        return BK_ERR_NO_MEM;
    }

    return BK_OK;
}

pet_prompt_result_t pet_prompt_play(const char *prompt_id,
                                    const pet_prompt_gate_t *gate,
                                    pet_prompt_skip_reason_t *skip_reason)
{
    char path[PET_PROMPT_PATH_MAX] = {0};
    bk_err_t ret = BK_OK;

    if (skip_reason != NULL) {
        *skip_reason = PET_PROMPT_SKIP_NONE;
    }

    if (prompt_id == NULL || prompt_id[0] == '\0') {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_EMPTY_ID;
        }
        LOGI("skip id=(empty) reason=%s\r\n", pet_prompt_skip_reason_name(PET_PROMPT_SKIP_EMPTY_ID));
        return PET_PROMPT_RESULT_SKIPPED;
    }

    if (gate != NULL && gate->tts_active && !gate->can_interrupt_tts) {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_TTS_ACTIVE;
        }
        LOGI("skip id=%s reason=%s\r\n", prompt_id, pet_prompt_skip_reason_name(PET_PROMPT_SKIP_TTS_ACTIVE));
        return PET_PROMPT_RESULT_SKIPPED;
    }

    ret = pet_prompt_path_for_id(prompt_id, path, sizeof(path));
    if (ret != BK_OK) {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_NOT_ALLOWED;
        }
        LOGW("skip id=%s reason=%s ret=%d\r\n",
             prompt_id,
             pet_prompt_skip_reason_name(PET_PROMPT_SKIP_NOT_ALLOWED),
             ret);
        return PET_PROMPT_RESULT_SKIPPED;
    }

    ret = dialog_module_instance()->speaker_play_prompt_tone(path);
    if (ret != BK_OK) {
        LOGW("miss id=%s path=%s ret=%d\r\n", prompt_id, path, ret);
        return PET_PROMPT_RESULT_MISSING;
    }

    LOGI("play id=%s path=%s\r\n", prompt_id, path);
    return PET_PROMPT_RESULT_PLAYED;
}

const char *pet_prompt_result_name(pet_prompt_result_t result)
{
    switch (result) {
    case PET_PROMPT_RESULT_PLAYED:
        return "played";
    case PET_PROMPT_RESULT_SKIPPED:
        return "skipped";
    case PET_PROMPT_RESULT_MISSING:
        return "missing";
    case PET_PROMPT_RESULT_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

const char *pet_prompt_skip_reason_name(pet_prompt_skip_reason_t reason)
{
    switch (reason) {
    case PET_PROMPT_SKIP_NONE:
        return "none";
    case PET_PROMPT_SKIP_EMPTY_ID:
        return "empty_id";
    case PET_PROMPT_SKIP_TTS_ACTIVE:
        return "tts_active";
    case PET_PROMPT_SKIP_NOT_ALLOWED:
        return "not_allowed";
    default:
        return "unknown";
    }
}
```

- [ ] **Step 3: 注册 `pet_prompt.c`**

在 `ap/CMakeLists.txt` 中，把这个源码加入现有 pet 模块旁边：

```cmake
    ./main/pet/pet_prompt.c
```

- [ ] **Step 4: 添加 prompt CLI 命令**

在 `cli_app_pet.c` 中包含 prompt 头文件：

```c
#include "pet_prompt.h"
```

更新 help 文本，加入：

```c
    CLI_RAW_LOGI("  pet prompt play <id>\r\n");
```

加入这个辅助函数：

```c
static void cli_pet_prompt_cmd(int argc, char **argv)
{
    pet_prompt_gate_t gate = {
        .tts_active = false,
        .can_interrupt_tts = true,
    };
    pet_prompt_skip_reason_t skip_reason = PET_PROMPT_SKIP_NONE;
    pet_prompt_result_t result = PET_PROMPT_RESULT_ERROR;

    if (argc != 4 || os_strcmp(argv[2], "play") != 0) {
        CLI_LOGE("pet prompt play <id>\r\n");
        return;
    }

    result = pet_prompt_play(argv[3], &gate, &skip_reason);
    CLI_LOGI("pet prompt result=%s skip=%s id=%s\r\n",
             pet_prompt_result_name(result),
             pet_prompt_skip_reason_name(skip_reason),
             argv[3]);
}
```

在命令分发处加入：

```c
    } else if (os_strcmp(argv[1], "prompt") == 0) {
        cli_pet_prompt_cmd(argc, argv);
```

- [ ] **Step 5: 运行静态护栏**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

期望：prompt 注册相关检查通过；behavior runtime 相关测试仍然失败。

---

### Task 3: Behavior Runtime 骨架和输出仲裁

**文件：**
- 新增： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_behavior_runtime.h`
- 新增： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_behavior_runtime.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`

- [ ] **Step 1: 创建 `pet_behavior_runtime.h`**

```c
#ifndef __PET_BEHAVIOR_RUNTIME_H__
#define __PET_BEHAVIOR_RUNTIME_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#include "pet_action_router.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_BEHAVIOR_SOURCE_LOCAL = 0,
    PET_BEHAVIOR_SOURCE_CLOUD,
    PET_BEHAVIOR_SOURCE_MCP,
    PET_BEHAVIOR_SOURCE_CLI,
} pet_behavior_source_t;

typedef enum {
    PET_BEHAVIOR_SAFETY_NORMAL = 0,
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

typedef struct {
    bool tts_active;
    uint32_t last_eye_ms;
    uint32_t last_prompt_ms;
    uint32_t last_haptic_ms;
    char last_emotion[16];
    char last_prompt_id[32];
    char last_haptic[16];
} pet_behavior_runtime_status_t;

bk_err_t pet_behavior_runtime_init(void);
bk_err_t pet_behavior_runtime_execute(const pet_behavior_request_t *request);
void pet_behavior_runtime_set_cloud_tts_active(bool active);
void pet_behavior_runtime_get_status(pet_behavior_runtime_status_t *status);
const char *pet_behavior_source_name(pet_behavior_source_t source);
const char *pet_behavior_safety_name(pet_behavior_safety_t safety);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: 创建 `pet_behavior_runtime.c`**

```c
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>
#include <components/log.h>

#include "app_ui.h"
#include "dialog_module.h"
#include "protocol.h"
#include "pet_behavior_runtime.h"
#include "pet_haptic.h"
#include "pet_prompt.h"

#define TAG "pet_behavior"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define PET_BEHAVIOR_EYE_COOLDOWN_MS        300
#define PET_BEHAVIOR_PROMPT_COOLDOWN_MS     500
#define PET_BEHAVIOR_HAPTIC_COOLDOWN_MS     500
#define PET_BEHAVIOR_SAFETY_COOLDOWN_MS     100

static pet_behavior_runtime_status_t s_behavior = {0};

static bool pet_behavior_is_safety(const pet_behavior_request_t *request)
{
    return request != NULL &&
           (request->safety == PET_BEHAVIOR_SAFETY_SAFETY ||
            request->safety == PET_BEHAVIOR_SAFETY_PRIVACY ||
            request->can_interrupt_tts);
}

static bool pet_behavior_cooldown_elapsed(uint32_t last_ms, uint32_t cooldown_ms, uint32_t now_ms)
{
    return last_ms == 0 || (now_ms - last_ms) >= cooldown_ms;
}

static void pet_behavior_copy_string(char *dst, uint32_t dst_len, const char *src)
{
    if (dst == NULL || dst_len == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    os_strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

bk_err_t pet_behavior_runtime_init(void)
{
    os_memset(&s_behavior, 0, sizeof(s_behavior));
    LOGI("runtime init\r\n");
    return BK_OK;
}

void pet_behavior_runtime_set_cloud_tts_active(bool active)
{
    s_behavior.tts_active = active;
    LOGI("tts_active=%d\r\n", active);
}

static void pet_behavior_execute_abort_if_needed(const pet_behavior_request_t *request)
{
    if (request->action != PET_ACTION_ABORT) {
        return;
    }

    dialog_module_instance()->speaker_play_abort();
    if (request->send_cloud_abort &&
        protocol_instance()->m_is_init &&
        protocol_instance()->m_is_start &&
        protocol_instance()->sendAbortListen != NULL) {
        protocol_instance()->sendAbortListen((uint8_t *)"pet_abort");
    }
}

bk_err_t pet_behavior_runtime_execute(const pet_behavior_request_t *request)
{
    uint32_t now_ms = rtos_get_time();
    const char *emotion = NULL;
    bool safety = false;

    if (request == NULL || request->action == PET_ACTION_NONE) {
        return BK_ERR_PARAM;
    }

    safety = pet_behavior_is_safety(request);
    emotion = pet_action_router_normalize_emotion(request->emotion, "smiling");

    LOGI("accept action=%s source=%s safety=%s emotion=%s prompt=%s haptic=%s tts_active=%d\r\n",
         pet_action_router_action_name(request->action),
         pet_behavior_source_name(request->source),
         pet_behavior_safety_name(request->safety),
         emotion,
         request->prompt_id ? request->prompt_id : "",
         request->haptic ? request->haptic : "",
         s_behavior.tts_active);

    pet_behavior_execute_abort_if_needed(request);

    if (emotion != NULL &&
        (safety || pet_behavior_cooldown_elapsed(s_behavior.last_eye_ms,
                                                 PET_BEHAVIOR_EYE_COOLDOWN_MS,
                                                 now_ms))) {
        app_ui_display_chat_emotion((char *)emotion);
        s_behavior.last_eye_ms = now_ms;
        pet_behavior_copy_string(s_behavior.last_emotion, sizeof(s_behavior.last_emotion), emotion);
    }

    if (request->prompt_id != NULL && request->prompt_id[0] != '\0' &&
        (safety || pet_behavior_cooldown_elapsed(s_behavior.last_prompt_ms,
                                                 PET_BEHAVIOR_PROMPT_COOLDOWN_MS,
                                                 now_ms))) {
        pet_prompt_gate_t gate = {
            .tts_active = s_behavior.tts_active,
            .can_interrupt_tts = request->can_interrupt_tts,
        };
        pet_prompt_skip_reason_t skip_reason = PET_PROMPT_SKIP_NONE;
        pet_prompt_result_t prompt_result = pet_prompt_play(request->prompt_id, &gate, &skip_reason);
        if (prompt_result == PET_PROMPT_RESULT_PLAYED) {
            s_behavior.last_prompt_ms = now_ms;
            pet_behavior_copy_string(s_behavior.last_prompt_id,
                                     sizeof(s_behavior.last_prompt_id),
                                     request->prompt_id);
        }
    }

    if (request->haptic != NULL && request->haptic[0] != '\0' &&
        (safety || pet_behavior_cooldown_elapsed(s_behavior.last_haptic_ms,
                                                 safety ? PET_BEHAVIOR_SAFETY_COOLDOWN_MS : PET_BEHAVIOR_HAPTIC_COOLDOWN_MS,
                                                 now_ms))) {
        bk_err_t haptic_ret = pet_haptic_play(request->haptic, 0);
        if (haptic_ret == BK_OK) {
            s_behavior.last_haptic_ms = now_ms;
            pet_behavior_copy_string(s_behavior.last_haptic,
                                     sizeof(s_behavior.last_haptic),
                                     request->haptic);
        } else {
            LOGW("haptic pattern failed ret=%d\r\n", haptic_ret);
        }
    }

    return BK_OK;
}

void pet_behavior_runtime_get_status(pet_behavior_runtime_status_t *status)
{
    if (status == NULL) {
        return;
    }

    *status = s_behavior;
}

const char *pet_behavior_source_name(pet_behavior_source_t source)
{
    switch (source) {
    case PET_BEHAVIOR_SOURCE_LOCAL:
        return "local";
    case PET_BEHAVIOR_SOURCE_CLOUD:
        return "cloud";
    case PET_BEHAVIOR_SOURCE_MCP:
        return "mcp";
    case PET_BEHAVIOR_SOURCE_CLI:
        return "cli";
    default:
        return "unknown";
    }
}

const char *pet_behavior_safety_name(pet_behavior_safety_t safety)
{
    switch (safety) {
    case PET_BEHAVIOR_SAFETY_NORMAL:
        return "normal";
    case PET_BEHAVIOR_SAFETY_USER_FEEDBACK:
        return "user_feedback";
    case PET_BEHAVIOR_SAFETY_SAFETY:
        return "safety";
    case PET_BEHAVIOR_SAFETY_PRIVACY:
        return "privacy";
    default:
        return "unknown";
    }
}
```

- [ ] **Step 3: 注册 `pet_behavior_runtime.c`**

在 `ap/CMakeLists.txt` 中加入：

```cmake
    ./main/pet/pet_behavior_runtime.c
```

- [ ] **Step 4: 初始化 runtime**

在启动阶段调用 `pet_brain_init()` 的位置，加入：

```c
pet_behavior_runtime_init();
```

如果当前启动代码只初始化 `pet_brain`，同时包含：

```c
#include "pet_behavior_runtime.h"
```

- [ ] **Step 5: 添加 behavior status CLI**

在 `cli_app_pet.c` 中包含：

```c
#include "pet_behavior_runtime.h"
```

加入：

```c
static void cli_pet_behavior_status(void)
{
    pet_behavior_runtime_status_t status = {0};

    pet_behavior_runtime_get_status(&status);
    CLI_LOGI("pet behavior tts_active=%d last_eye_ms=%u last_prompt_ms=%u last_haptic_ms=%u last_emotion=%s last_prompt=%s last_haptic=%s\r\n",
             status.tts_active,
             status.last_eye_ms,
             status.last_prompt_ms,
             status.last_haptic_ms,
             status.last_emotion,
             status.last_prompt_id,
             status.last_haptic);
}
```

在 `pet behavior` 分发逻辑中加入 `status` 支持：

```c
    if (argc == 3 && os_strcmp(argv[2], "status") == 0) {
        cli_pet_behavior_status();
        return;
    }
```

- [ ] **Step 6: 运行静态护栏**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

期望：新 runtime 注册和热路径护栏通过。router/brain 委托完成前，部分迁移护栏仍可能失败。

---

### Task 4: 将 Cloud 和 CLI Action 路由到 Behavior Runtime

**文件：**
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.h`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/iot/iot_pet.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`

- [ ] **Step 1: 为 cloud action request 增加 runtime 字段**

在 `pet_action_router.h` 中扩展 `pet_action_request_t`：

```c
typedef struct {
    pet_action_type_t action;
    const char *emotion;
    const char *haptic;
    const char *prompt_id;
    uint32_t duration_ms;
    pet_local_delta_t local_delta;
    bool send_cloud_abort;
    bool can_interrupt_tts;
    bool requires_online;
} pet_action_request_t;
```

- [ ] **Step 2: 从 JSON 解析 `prompt_id` 和 flags**

在 `pet_action_router_request_from_json()` 中增加局部变量：

```c
    cJSON *prompt_id = NULL;
    cJSON *can_interrupt_tts = NULL;
    cJSON *requires_online = NULL;
```

在 haptic 解析后加入：

```c
    prompt_id = cJSON_GetObjectItemCaseSensitive(root, "prompt_id");
    if (cJSON_IsString(prompt_id)) {
        request->prompt_id = prompt_id->valuestring;
    }
```

在 `send_cloud_abort` 之后加入：

```c
    can_interrupt_tts = cJSON_GetObjectItemCaseSensitive(root, "can_interrupt_tts");
    if (cJSON_IsBool(can_interrupt_tts)) {
        request->can_interrupt_tts = cJSON_IsTrue(can_interrupt_tts);
    }

    requires_online = cJSON_GetObjectItemCaseSensitive(root, "requires_online");
    if (cJSON_IsBool(requires_online)) {
        request->requires_online = cJSON_IsTrue(requires_online);
    }
```

- [ ] **Step 3: 用 runtime 委托替换 router 直接执行**

在 `pet_action_router.c` 中移除对 `app_ui.h`、`dialog_module.h`、`protocol.h` 和 `pet_haptic.h` 的直接 include；加入：

```c
#include "pet_behavior_runtime.h"
```

把 `pet_action_router_execute()` 替换为：

```c
bk_err_t pet_action_router_execute(const pet_action_request_t *request)
{
    pet_behavior_request_t behavior = {0};

    if (request == NULL || request->action == PET_ACTION_NONE) {
        return BK_ERR_PARAM;
    }

    behavior.action = request->action;
    behavior.emotion = pet_action_router_normalize_emotion(request->emotion, "smiling");
    behavior.prompt_id = request->prompt_id;
    behavior.haptic = request->haptic;
    behavior.duration_ms = pet_action_router_clip_duration(request->duration_ms);
    behavior.local_delta = request->local_delta;
    behavior.source = PET_BEHAVIOR_SOURCE_CLOUD;
    behavior.safety = (request->action == PET_ACTION_ABORT ||
                       request->action == PET_ACTION_PRIVACY_ON ||
                       request->action == PET_ACTION_PRIVACY_OFF)
                          ? PET_BEHAVIOR_SAFETY_PRIVACY
                          : PET_BEHAVIOR_SAFETY_NORMAL;
    behavior.send_cloud_abort = request->send_cloud_abort;
    behavior.can_interrupt_tts = request->can_interrupt_tts ||
                                 request->action == PET_ACTION_ABORT ||
                                 request->action == PET_ACTION_PRIVACY_ON ||
                                 request->action == PET_ACTION_PRIVACY_OFF;
    behavior.requires_online = request->requires_online;

    LOGI("execute action=%s emotion=%s prompt=%s duration=%u haptic=%s\r\n",
         pet_action_router_action_name(request->action),
         behavior.emotion,
         behavior.prompt_id ? behavior.prompt_id : "",
         behavior.duration_ms,
         behavior.haptic ? behavior.haptic : "");

    return pet_behavior_runtime_execute(&behavior);
}
```

- [ ] **Step 4: MCP 保持语义化控制**

在 `iot_pet.c` 中继续使用现有 `pet_action_router_request_from_json()`。不要增加任何原始硬件工具字段。如果要更新工具 schema 文本，允许的 JSON 仍保持：

```json
{
  "action": "emote",
  "emotion": "happy",
  "prompt_id": "happy_chirp",
  "haptic": "tap",
  "duration_ms": 800,
  "local_delta": {"mood": 1}
}
```

- [ ] **Step 5: 运行静态护栏**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

期望：router 热路径护栏通过，且 `pet_action_router.c` 不再包含直接 display/protocol/haptic 执行逻辑。

---

### Task 5: 将本地 Brain Event 迁移为 Behavior Request

**文件：**
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.h`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`

- [ ] **Step 1: 包含 runtime 和 prompt 头文件**

在 `pet_brain.c` 中加入：

```c
#include "pet_behavior_runtime.h"
#include "pet_prompt.h"
```

- [ ] **Step 2: 添加本地 behavior 执行辅助函数**

在 `pet_brain_execute()` 附近加入这个辅助函数：

```c
static bk_err_t pet_brain_execute_behavior(pet_state_t next_state,
                                           const pet_behavior_request_t *request)
{
    bk_err_t ret = BK_OK;

    if (request == NULL) {
        return BK_ERR_PARAM;
    }

    ret = pet_behavior_runtime_execute(request);
    if (ret == BK_OK) {
        s_pet.state = next_state;
        s_pet.last_action = request->action;
        os_strncpy(s_pet.last_emotion,
                   pet_action_router_normalize_emotion(request->emotion, "smiling"),
                   sizeof(s_pet.last_emotion) - 1);
        s_pet.last_emotion[sizeof(s_pet.last_emotion) - 1] = '\0';
        pet_brain_apply_delta(&request->local_delta);
    }

    return ret;
}
```

- [ ] **Step 3: 修改 `pet_brain_emit_emotion()` 使用 runtime**

将函数体替换为：

```c
bk_err_t pet_brain_emit_emotion(const char *emotion)
{
    pet_behavior_request_t request = {
        .action = PET_ACTION_EMOTE,
        .emotion = emotion,
        .duration_ms = 2000,
        .source = PET_BEHAVIOR_SOURCE_CLI,
        .safety = PET_BEHAVIOR_SAFETY_USER_FEEDBACK,
    };

    return pet_brain_execute_behavior(PET_STATE_LOCAL_REACT, &request);
}
```

- [ ] **Step 4: 在 audio event 中设置 TTS active 标志**

在 `pet_brain_handle_event()` 的 TTS 分支中调用 runtime 状态设置：

```c
    case PET_EVENT_AUDIO_TTS_START:
        pet_behavior_runtime_set_cloud_tts_active(true);
        request.action = PET_ACTION_REPLY;
        request.emotion = "smiling";
        next_state = PET_STATE_CLOUD_SPEAK;
        break;

    case PET_EVENT_AUDIO_TTS_STOP:
        pet_behavior_runtime_set_cloud_tts_active(false);
        request.emotion = "smiling";
        next_state = PET_STATE_IDLE_ALIVE;
        break;
```

- [ ] **Step 5: 转换本地 event 映射，加入 prompt 和 safety 信息**

在 `pet_brain_handle_event()` 中，将原来的 `pet_action_request_t request` 迁移为 `pet_behavior_request_t request`。初始声明应为：

```c
    pet_behavior_request_t request = {
        .action = PET_ACTION_EMOTE,
        .emotion = "smiling",
        .duration_ms = 2000,
        .source = PET_BEHAVIOR_SOURCE_LOCAL,
        .safety = PET_BEHAVIOR_SAFETY_NORMAL,
    };
```

主要本地分支按下面映射：

```c
    case PET_EVENT_TOUCH_HEAD_SHORT:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_HAPPY_CHIRP();
        request.haptic = "tap";
        request.local_delta.affection = 1;
        request.local_delta.mood = 1;
        break;

    case PET_EVENT_TOUCH_HEAD_LONG:
        request.action = PET_ACTION_SOOTHE;
        request.emotion = "caring";
        request.prompt_id = "comforted";
        request.haptic = "soft";
        request.local_delta.stress = -5;
        request.local_delta.affection = 2;
        next_state = PET_STATE_SOOTHE;
        break;

    case PET_EVENT_TOUCH_CHIN_SHORT:
        request.emotion = "curious";
        request.prompt_id = PET_PROMPT_ID_CURIOUS();
        request.local_delta.novelty = 1;
        break;

    case PET_EVENT_TOUCH_CHIN_LONG:
    case PET_EVENT_AUDIO_ABORT:
        request.action = PET_ACTION_ABORT;
        request.emotion = "confused";
        request.prompt_id = PET_PROMPT_ID_CANCEL();
        request.haptic = "tap";
        request.send_cloud_abort = true;
        request.can_interrupt_tts = true;
        request.safety = PET_BEHAVIOR_SAFETY_PRIVACY;
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_TOUCH_CHIN_VERY_LONG:
    case PET_EVENT_PRIVACY_ON:
        s_pet.privacy = true;
        request.action = PET_ACTION_PRIVACY_ON;
        request.emotion = "doubtful";
        request.prompt_id = PET_PROMPT_ID_PRIVACY_ON();
        request.haptic = "confirm";
        request.can_interrupt_tts = true;
        request.safety = PET_BEHAVIOR_SAFETY_PRIVACY;
        next_state = PET_STATE_PRIVACY;
        break;

    case PET_EVENT_MOTION_FREEFALL:
    case PET_EVENT_MOTION_IMPACT:
        request.action = PET_ACTION_SOOTHE;
        request.emotion = "afraid";
        request.prompt_id = PET_PROMPT_ID_IMPACT();
        request.haptic = "alert";
        request.can_interrupt_tts = true;
        request.safety = PET_BEHAVIOR_SAFETY_SAFETY;
        request.local_delta.stress = 5;
        request.local_delta.security = -5;
        next_state = PET_STATE_SOOTHE;
        break;
```

在 `pet_brain_handle_event()` 结尾调用：

```c
    return pet_brain_execute_behavior(next_state, &request);
```

- [ ] **Step 6: 添加 `pet behavior event` CLI**

In `cli_app_pet.c`, add:

```c
static void cli_pet_behavior_cmd(int argc, char **argv)
{
    if (argc < 3) {
        CLI_LOGE("pet behavior status|event <event>|json <json>\r\n");
        return;
    }

    if (os_strcmp(argv[2], "status") == 0) {
        cli_pet_behavior_status();
    } else if (os_strcmp(argv[2], "event") == 0) {
        pet_event_type_t event = PET_EVENT_NONE;
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet behavior event <event>\r\n");
            return;
        }

        event = pet_brain_event_from_name(argv[3]);
        if (event == PET_EVENT_NONE) {
            CLI_LOGE("unknown pet event: %s\r\n", argv[3]);
            return;
        }

        ret = pet_brain_handle_event(event);
        CLI_LOGI("pet behavior event=%s ret=%d\r\n", argv[3], ret);
    } else if (os_strcmp(argv[2], "json") == 0) {
        if (argc != 4) {
            CLI_LOGE("pet behavior json <json>\r\n");
            return;
        }
        cli_pet_action_json(argv[3]);
    } else {
        CLI_LOGE("pet behavior status|event <event>|json <json>\r\n");
    }
}
```

如果 `cli_pet_action_json()` 还不存在，就把现有 `pet action <json>` 的 JSON 分支提取成：

```c
static void cli_pet_action_json(const char *json)
{
    cJSON *root = NULL;
    pet_action_request_t request = {0};
    bk_err_t ret = BK_OK;

    root = cJSON_Parse(json);
    if (root == NULL) {
        CLI_LOGE("invalid pet action json\r\n");
        return;
    }

    ret = pet_action_router_request_from_json(root, &request);
    if (ret == BK_OK) {
        cli_pet_apply_action_request(&request);
    } else {
        CLI_LOGE("pet action parse failed: %d\r\n", ret);
    }

    cJSON_Delete(root);
}
```

- [ ] **Step 7: 运行护栏并构建**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

期望：静态护栏通过，固件构建完成。

---

### Task 6: 在线生命周期 Prompt 桥接

**文件：**
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/system_manager/system_manager.c`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/iot/iot_camera.c`

- [ ] **Step 1: 给 voice 和 cloud event 添加 prompt ID**

在 `pet_brain_handle_event()` 中使用这些映射：

```c
    case PET_EVENT_AUDIO_LISTEN_START:
        if (s_pet.privacy) {
            request.emotion = "doubtful";
            next_state = PET_STATE_PRIVACY;
            break;
        }
        request.emotion = "curious";
        request.prompt_id = PET_PROMPT_ID_LISTEN_START();
        next_state = PET_STATE_CLOUD_LISTEN;
        break;

    case PET_EVENT_CLOUD_CONNECTED:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_NET_OK();
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_CLOUD_ERROR:
        request.emotion = "confused";
        request.prompt_id = PET_PROMPT_ID_NET_LOST();
        next_state = PET_STATE_ERROR_DEGRADED;
        break;
```

- [ ] **Step 2: 添加 vision event enum**

在 `pet_brain.h` 的 privacy events 前加入：

```c
    PET_EVENT_VISION_CAPTURE_START,
    PET_EVENT_VISION_CAPTURE_DONE,
    PET_EVENT_VISION_CAPTURE_ERROR,
```

在 `s_event_names` 中加入：

```c
    {PET_EVENT_VISION_CAPTURE_START, "vision_capture_start"},
    {PET_EVENT_VISION_CAPTURE_DONE, "vision_capture_done"},
    {PET_EVENT_VISION_CAPTURE_ERROR, "vision_capture_error"},
```

在 `pet_brain_handle_event()` 中加入：

```c
    case PET_EVENT_VISION_CAPTURE_START:
        request.emotion = "photo";
        request.prompt_id = PET_PROMPT_ID_PHOTO();
        next_state = PET_STATE_LOCAL_REACT;
        break;

    case PET_EVENT_VISION_CAPTURE_DONE:
        request.emotion = "happy";
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_VISION_CAPTURE_ERROR:
        request.emotion = "confused";
        request.prompt_id = PET_PROMPT_ID_ERROR();
        next_state = PET_STATE_ERROR_DEGRADED;
        break;
```

- [ ] **Step 3: 从 camera tool 发布 vision 生命周期事件**

在 `iot_camera.c` 的 capture/upload 入口附近加入：

```c
pet_brain_handle_event(PET_EVENT_VISION_CAPTURE_START);
```

成功时加入：

```c
pet_brain_handle_event(PET_EVENT_VISION_CAPTURE_DONE);
```

抓拍或上传失败时加入：

```c
pet_brain_handle_event(PET_EVENT_VISION_CAPTURE_ERROR);
```

如果 `iot_camera.c` 还没有包含 pet brain，加入：

```c
#include "pet_brain.h"
```

- [ ] **Step 4: 构建**

运行：

```bash
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

期望：构建完成。如果包含 `pet_brain.h` 出现 include path 问题，把 `./main/pet` 加到相关 component include list，不要使用相对路径绕过。

---

### Task 7: Prompt 资源文档

**文件：**
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/resources/prompts/README.md`
- 修改： `/home/jason/armino1/cmaiw82al_ai_toy/docs/08_ai_pet_interaction_core_plan.md`

- [ ] **Step 1: 用 manifest 更新 prompt README**

把当前简略 prompt 段落替换为：

````markdown
## Runtime Path

Firmware resolves prompt IDs to:

```text
/if0/prompts/pet/<prompt_id>.mp3
```

## Initial Prompt Manifest

| ID | Purpose | Target duration |
| --- | --- | --- |
| `boot` | startup / wake from sleep | 500-900 ms |
| `net_ok` | cloud connected | 300-700 ms |
| `net_lost` | cloud disconnected or degraded | 400-900 ms |
| `low_power` | low battery warning | 600-1200 ms |
| `error` | recoverable error | 400-900 ms |
| `listen_start` | about to listen | 250-600 ms |
| `cancel` | cancel / abort acknowledgement | 250-600 ms |
| `photo` | camera capture started | 300-800 ms |
| `done` | local action completed | 250-600 ms |
| `happy_chirp` | happy local reaction | 200-600 ms |
| `curious` | curious local reaction | 250-700 ms |
| `afraid` | afraid reaction | 400-900 ms |
| `comforted` | soothe completed | 400-900 ms |
| `impact` | impact/freefall alert | 300-800 ms |
| `privacy_on` | privacy enabled | 300-700 ms |
| `privacy_off` | privacy disabled | 300-700 ms |
| `stop` | hard stop | 200-500 ms |
| `test_wake_prompt` | operator cue before user speech/action tests | 500-1200 ms |
````

- [ ] **Step 2: 在核心计划中增加 runtime 阶段说明**

在 `docs/08_ai_pet_interaction_core_plan.md` 的 Phase 5 后增加新阶段：

```markdown
### Phase 6: Pet Interaction Runtime

目标：把触摸、姿态、语音、云端、MCP、识图、隐私和安全事件统一为
`pet_behavior_request_t`，由 `pet_behavior_runtime` 仲裁眼神、提示音、震动、
云端 abort 和本地变量更新。

- 新增 `pet_prompt.[ch]`：提示音 ID manifest、路径生成和 TTS 互斥播放门控。
- 新增 `pet_behavior_runtime.[ch]`：输出优先级、节流和统一执行。
- `pet_action_router` 保留云端白名单解析，但不再直接操作 eye/haptic/abort。
- `pet_brain` 保持本地权威，离线事件表优先闭合。
- 本阶段不训练离线唤醒词，不默认开启业务振动硬件输出，不改 AVI/JPEG/display 热路径。
```

- [ ] **Step 3: 搜索未完成标记文本**

运行：

```bash
cd /home/jason/armino1
rg -n "PLACEHOLDER|UNFINISHED|待补充|未完成" cmaiw82al_ai_toy/resources/prompts/README.md cmaiw82al_ai_toy/docs/08_ai_pet_interaction_core_plan.md || true
```

期望：本任务没有引入未完成标记文本。

---

### Task 8: 主机侧验证

**文件：**
- 不修改源码。

- [ ] **Step 1: 运行静态护栏**

运行：

```bash
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

期望：所有测试通过，包括：

```text
test_pet_interaction_runtime_modules_are_registered ... ok
test_pet_behavior_runtime_does_not_touch_display_hot_paths ... ok
test_pet_prompt_is_gated_against_cloud_tts ... ok
```

- [ ] **Step 2: 构建固件**

运行：

```bash
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

期望：构建完成并产出：

```text
build/bk7258/cmaiw82al_ai_toy/all-app.bin
build/bk7258/cmaiw82al_ai_toy/app_pack.rbl
```

- [ ] **Step 3: 汇总构建结果**

运行：

```bash
python3 /home/jason/.codex/skills/bk7258-xiaozhi-device-dev/scripts/summarize_bk7258_build.py
```

期望：汇总结果中没有 fatal build errors。

---

### Task 9: 上板验证

**文件：**
- 除非验证暴露出具体缺陷，否则不修改源码。

- [ ] **Step 1: 检查串口占用**

运行：

```bash
lsof /dev/ttyUSB0 || true
```

期望：没有无关进程占用 `/dev/ttyUSB0`。如果已有 logger 在运行，先停止它，再打开新的串口日志。

- [ ] **Step 2: OTA 固件**

使用 `bk7258-ota-flash-debug` 中已经建立的 BK7258 OTA 流程。启动当前 `app_pack.rbl` 的 HTTP 服务，在板端执行 `update ota <url>`，并抓取完整串口日志。

期望日志标记：

```text
APP_OTA begin
cyg_recvlen_per:(100.00)%
write over
```

- [ ] **Step 3: 验证 prompt 和 behavior CLI**

串口执行：

```text
pet status
pet prompt play test_wake_prompt
pet behavior status
pet behavior event touch_head_short
pet behavior event touch_head_long
pet behavior event motion_impact
pet behavior json {"action":"emote","emotion":"happy","prompt_id":"happy_chirp","haptic":"tap","duration_ms":800}
pet behavior status
```

期望标记：

```text
pet_prompt play id=test_wake_prompt
pet_behavior accept action=emote
pet_behavior accept action=soothe
pet haptic
EYE_SWITCH request
EYE_AVI_FPS avg=
```

如果 prompt 文件还没安装，可能出现 `pet_prompt miss`；这在真实资源安装前可接受。缺文件不能导致崩溃或阻塞眼神播放。

- [ ] **Step 4: 验证 TTS prompt 仲裁**

运行：

```text
pet voice connect
pet voice tts_start
pet behavior json {"action":"emote","emotion":"happy","prompt_id":"happy_chirp","duration_ms":800}
pet voice tts_stop
pet behavior json {"action":"emote","emotion":"happy","prompt_id":"happy_chirp","duration_ms":800}
```

期望：

```text
tts_active=1
pet_prompt skip id=happy_chirp reason=tts_active
tts_active=0
```

第二次 behavior 是否播放或 miss prompt，取决于文件是否存在。

- [ ] **Step 5: 验证没有 display 回归**

behavior 测试后，让板子空闲至少 60 秒。

期望：

```text
EYE_AVI_FPS avg=24.
errors=0
overruns=0
```

如果日志包含以下内容，拒绝该构建：

```text
Assert at
HardFault
rtos_dump_system
EYE_FRAME_COLOR_ALERT
yellow
blue
```

- [ ] **Step 6: 记录证据**

把 OTA 输出目录、串口日志路径和上板验证发现的残余风险更新到 `docs/08_ai_pet_interaction_core_plan.md`。

---

## 计划自检

- Spec 覆盖：行为包、prompt 设计、离线表、在线桥、模块边界、仲裁和验证都已映射到任务。
- 未完成标记扫描：本计划避免 placeholder 和开放式实现描述。
- 类型一致性：`pet_behavior_request_t`、`pet_prompt_gate_t` 和 runtime status 名称都在后续任务使用前定义。
- 范围：本文档编写步骤不执行固件实现；第一个实现任务从静态护栏开始。
