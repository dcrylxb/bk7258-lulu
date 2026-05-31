#include <os/mem.h>
#include <os/str.h>
#include <components/log.h>

#include "pet_behavior_runtime.h"
#include "pet_action_router.h"

#define TAG "pet_router"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    const char *name;
    pet_action_type_t action;
} pet_action_name_t;

static const char *s_allowed_emotions[] = {
    "afraid",
    "angry",
    "bored",
    "caring",
    "confused",
    "curious",
    "doubtful",
    "frowning",
    "grimacing",
    "happy",
    "photo",
    "sad",
    "smiling",
    "surprised",
    "tired",
    "winking",
};

static const pet_action_name_t s_action_names[] = {
    {"reply", PET_ACTION_REPLY},
    {"emote", PET_ACTION_EMOTE},
    {"soothe", PET_ACTION_SOOTHE},
    {"sleep", PET_ACTION_SLEEP},
    {"privacy_on", PET_ACTION_PRIVACY_ON},
    {"privacy_off", PET_ACTION_PRIVACY_OFF},
    {"abort", PET_ACTION_ABORT},
};

bool pet_action_router_emotion_is_allowed(const char *emotion)
{
    if (emotion == NULL || emotion[0] == '\0') {
        return false;
    }

    for (uint32_t i = 0; i < sizeof(s_allowed_emotions) / sizeof(s_allowed_emotions[0]); i++) {
        if (os_strcasecmp(emotion, s_allowed_emotions[i]) == 0) {
            return true;
        }
    }

    return false;
}

const char *pet_action_router_normalize_emotion(const char *emotion, const char *fallback)
{
    if (pet_action_router_emotion_is_allowed(emotion)) {
        return emotion;
    }

    if (pet_action_router_emotion_is_allowed(fallback)) {
        LOGW("unknown emotion fallback emotion=%s fallback=%s\r\n",
             emotion ? emotion : "(null)",
             fallback);
        return fallback;
    }

    LOGW("unknown emotion fallback emotion=%s fallback=smiling\r\n", emotion ? emotion : "(null)");
    return "smiling";
}

pet_action_type_t pet_action_router_action_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return PET_ACTION_NONE;
    }

    for (uint32_t i = 0; i < sizeof(s_action_names) / sizeof(s_action_names[0]); i++) {
        if (os_strcasecmp(name, s_action_names[i].name) == 0) {
            return s_action_names[i].action;
        }
    }

    return PET_ACTION_NONE;
}

const char *pet_action_router_action_name(pet_action_type_t action)
{
    for (uint32_t i = 0; i < sizeof(s_action_names) / sizeof(s_action_names[0]); i++) {
        if (s_action_names[i].action == action) {
            return s_action_names[i].name;
        }
    }

    return "none";
}

int8_t pet_action_router_clip_delta(int value)
{
    if (value > PET_ACTION_MAX_LOCAL_DELTA) {
        return PET_ACTION_MAX_LOCAL_DELTA;
    }
    if (value < -PET_ACTION_MAX_LOCAL_DELTA) {
        return -PET_ACTION_MAX_LOCAL_DELTA;
    }
    return (int8_t)value;
}

uint32_t pet_action_router_clip_duration(uint32_t duration_ms)
{
    if (duration_ms > PET_ACTION_MAX_DURATION_MS) {
        return PET_ACTION_MAX_DURATION_MS;
    }
    return duration_ms;
}

static int8_t pet_action_router_json_delta_value(cJSON *local_delta, const char *name)
{
    cJSON *item = NULL;

    if (local_delta == NULL) {
        return 0;
    }

    item = cJSON_GetObjectItemCaseSensitive(local_delta, name);
    if (!cJSON_IsNumber(item)) {
        return 0;
    }

    return pet_action_router_clip_delta(item->valueint);
}

bk_err_t pet_action_router_request_from_json(cJSON *root, pet_action_request_t *request)
{
    cJSON *action = NULL;
    cJSON *emotion = NULL;
    cJSON *haptic = NULL;
    cJSON *prompt_id = NULL;
    cJSON *duration = NULL;
    cJSON *send_cloud_abort = NULL;
    cJSON *can_interrupt_tts = NULL;
    cJSON *requires_online = NULL;
    cJSON *local_delta = NULL;

    if (root == NULL || request == NULL) {
        return BK_ERR_PARAM;
    }

    os_memset(request, 0, sizeof(*request));

    action = cJSON_GetObjectItemCaseSensitive(root, "action");
    if (!cJSON_IsString(action)) {
        LOGW("pet_action missing action\r\n");
        return BK_ERR_PARAM;
    }

    request->action = pet_action_router_action_from_name(action->valuestring);
    if (request->action == PET_ACTION_NONE) {
        LOGW("pet_action unknown action=%s\r\n", action->valuestring);
        return BK_ERR_NOT_SUPPORT;
    }

    emotion = cJSON_GetObjectItemCaseSensitive(root, "emotion");
    if (cJSON_IsString(emotion)) {
        request->emotion = pet_action_router_normalize_emotion(emotion->valuestring, "smiling");
    } else {
        request->emotion = "smiling";
    }

    haptic = cJSON_GetObjectItemCaseSensitive(root, "haptic");
    if (cJSON_IsString(haptic)) {
        request->haptic = haptic->valuestring;
    }

    prompt_id = cJSON_GetObjectItemCaseSensitive(root, "prompt_id");
    if (cJSON_IsString(prompt_id)) {
        request->prompt_id = prompt_id->valuestring;
    }

    duration = cJSON_GetObjectItemCaseSensitive(root, "duration_ms");
    if (cJSON_IsNumber(duration)) {
        request->duration_ms = pet_action_router_clip_duration((uint32_t)duration->valueint);
    }

    send_cloud_abort = cJSON_GetObjectItemCaseSensitive(root, "send_cloud_abort");
    if (cJSON_IsBool(send_cloud_abort)) {
        request->send_cloud_abort = cJSON_IsTrue(send_cloud_abort);
    }

    can_interrupt_tts = cJSON_GetObjectItemCaseSensitive(root, "can_interrupt_tts");
    if (cJSON_IsBool(can_interrupt_tts)) {
        request->can_interrupt_tts = cJSON_IsTrue(can_interrupt_tts);
    }

    requires_online = cJSON_GetObjectItemCaseSensitive(root, "requires_online");
    if (cJSON_IsBool(requires_online)) {
        request->requires_online = cJSON_IsTrue(requires_online);
    }

    local_delta = cJSON_GetObjectItemCaseSensitive(root, "local_delta");
    if (cJSON_IsObject(local_delta)) {
        request->local_delta.mood = pet_action_router_json_delta_value(local_delta, "mood");
        request->local_delta.energy = pet_action_router_json_delta_value(local_delta, "energy");
        request->local_delta.affection = pet_action_router_json_delta_value(local_delta, "affection");
        request->local_delta.security = pet_action_router_json_delta_value(local_delta, "security");
        request->local_delta.novelty = pet_action_router_json_delta_value(local_delta, "novelty");
        request->local_delta.stress = pet_action_router_json_delta_value(local_delta, "stress");
    }

    return BK_OK;
}

bk_err_t pet_action_router_execute(const pet_action_request_t *request)
{
    pet_behavior_request_t behavior = {0};

    if (request == NULL || request->action == PET_ACTION_NONE) {
        return BK_ERR_PARAM;
    }

    if (request->action != PET_ACTION_REPLY &&
        request->action != PET_ACTION_EMOTE &&
        request->action != PET_ACTION_SOOTHE &&
        request->action != PET_ACTION_SLEEP &&
        request->action != PET_ACTION_PRIVACY_ON &&
        request->action != PET_ACTION_PRIVACY_OFF &&
        request->action != PET_ACTION_ABORT) {
        LOGW("ignore unsupported action=%d\r\n", request->action);
        return BK_ERR_NOT_SUPPORT;
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
