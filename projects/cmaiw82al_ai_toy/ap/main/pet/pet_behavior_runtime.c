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
#define PET_BEHAVIOR_PROMPT_COOLDOWN_MS     2500
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
