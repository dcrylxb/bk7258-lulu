#include <os/mem.h>
#include <os/str.h>
#include <components/log.h>

#include "pet_brain.h"
#include "pet_behavior_runtime.h"
#include "pet_prompt.h"

#define TAG "pet_brain"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    pet_event_type_t event;
    const char *name;
} pet_event_name_t;

static pet_brain_snapshot_t s_pet = {0};

static const pet_event_name_t s_event_names[] = {
    {PET_EVENT_IDLE, "idle"},
    {PET_EVENT_TOUCH_HEAD_SHORT, "touch_head_short"},
    {PET_EVENT_TOUCH_HEAD_DOUBLE, "touch_head_double"},
    {PET_EVENT_TOUCH_HEAD_LONG, "touch_head_long"},
    {PET_EVENT_TOUCH_CHIN_SHORT, "touch_chin_short"},
    {PET_EVENT_TOUCH_CHIN_DOUBLE, "touch_chin_double"},
    {PET_EVENT_TOUCH_CHIN_LONG, "touch_chin_long"},
    {PET_EVENT_TOUCH_CHIN_VERY_LONG, "touch_chin_very_long"},
    {PET_EVENT_AUDIO_LISTEN_START, "audio_listen_start"},
    {PET_EVENT_AUDIO_LISTEN_STOP, "audio_listen_stop"},
    {PET_EVENT_AUDIO_TTS_START, "audio_tts_start"},
    {PET_EVENT_AUDIO_TTS_STOP, "audio_tts_stop"},
    {PET_EVENT_AUDIO_ABORT, "audio_abort"},
    {PET_EVENT_CLOUD_CONNECTED, "cloud_connected"},
    {PET_EVENT_CLOUD_ERROR, "cloud_error"},
    {PET_EVENT_VISION_CAPTURE_START, "vision_capture_start"},
    {PET_EVENT_VISION_CAPTURE_DONE, "vision_capture_done"},
    {PET_EVENT_VISION_CAPTURE_ERROR, "vision_capture_error"},
    {PET_EVENT_PRIVACY_ON, "privacy_on"},
    {PET_EVENT_PRIVACY_OFF, "privacy_off"},
    {PET_EVENT_SLEEP, "sleep"},
    {PET_EVENT_WAKE, "wake"},
    {PET_EVENT_MOTION_PICKED_UP, "motion_picked_up"},
    {PET_EVENT_MOTION_PUT_DOWN, "motion_put_down"},
    {PET_EVENT_MOTION_GENTLE_SHAKE, "motion_gentle_shake"},
    {PET_EVENT_MOTION_STRONG_SHAKE, "motion_strong_shake"},
    {PET_EVENT_MOTION_FREEFALL, "motion_freefall"},
    {PET_EVENT_MOTION_IMPACT, "motion_impact"},
    {PET_EVENT_MOTION_TILT_LEFT, "motion_tilt_left"},
    {PET_EVENT_MOTION_TILT_RIGHT, "motion_tilt_right"},
};

static void pet_brain_apply_delta(const pet_local_delta_t *delta)
{
    if (delta == NULL) {
        return;
    }

    s_pet.mood += pet_action_router_clip_delta(delta->mood);
    s_pet.energy += pet_action_router_clip_delta(delta->energy);
    s_pet.affection += pet_action_router_clip_delta(delta->affection);
    s_pet.security += pet_action_router_clip_delta(delta->security);
    s_pet.novelty += pet_action_router_clip_delta(delta->novelty);
    s_pet.stress += pet_action_router_clip_delta(delta->stress);
}

static bk_err_t pet_brain_execute(pet_state_t next_state, const pet_action_request_t *request)
{
    bk_err_t ret = BK_OK;

    if (request == NULL) {
        return BK_ERR_PARAM;
    }

    ret = pet_action_router_execute(request);
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

bk_err_t pet_brain_init(void)
{
    os_memset(&s_pet, 0, sizeof(s_pet));
    s_pet.state = PET_STATE_IDLE_ALIVE;
    s_pet.mood = 60;
    s_pet.energy = 70;
    s_pet.affection = 30;
    s_pet.security = 80;
    s_pet.novelty = 50;
    s_pet.stress = 5;
    os_strncpy(s_pet.last_emotion, "smiling", sizeof(s_pet.last_emotion) - 1);
    LOGI("pet brain init state=%s\r\n", pet_brain_state_name(s_pet.state));
    return BK_OK;
}

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

bk_err_t pet_brain_apply_action_request(const pet_action_request_t *request)
{
    pet_state_t next_state = PET_STATE_LOCAL_REACT;

    if (request == NULL) {
        return BK_ERR_PARAM;
    }

    switch (request->action) {
    case PET_ACTION_REPLY:
        next_state = PET_STATE_CLOUD_SPEAK;
        break;
    case PET_ACTION_EMOTE:
        next_state = PET_STATE_LOCAL_REACT;
        break;
    case PET_ACTION_SOOTHE:
        next_state = PET_STATE_SOOTHE;
        break;
    case PET_ACTION_SLEEP:
        next_state = PET_STATE_SLEEP;
        break;
    case PET_ACTION_PRIVACY_ON:
        s_pet.privacy = true;
        next_state = PET_STATE_PRIVACY;
        break;
    case PET_ACTION_PRIVACY_OFF:
        s_pet.privacy = false;
        next_state = PET_STATE_IDLE_ALIVE;
        break;
    case PET_ACTION_ABORT:
        next_state = PET_STATE_IDLE_ALIVE;
        break;
    default:
        return BK_ERR_NOT_SUPPORT;
    }

    LOGI("apply action=%s next=%s emotion=%s privacy=%d\r\n",
         pet_action_router_action_name(request->action),
         pet_brain_state_name(next_state),
         request->emotion ? request->emotion : "",
         s_pet.privacy);
    return pet_brain_execute(next_state, request);
}

bk_err_t pet_brain_set_privacy(bool enabled)
{
    return pet_brain_handle_event(enabled ? PET_EVENT_PRIVACY_ON : PET_EVENT_PRIVACY_OFF);
}

bk_err_t pet_brain_handle_event(pet_event_type_t event)
{
    pet_behavior_request_t request = {
        .action = PET_ACTION_EMOTE,
        .emotion = "smiling",
        .duration_ms = 2000,
        .source = PET_BEHAVIOR_SOURCE_LOCAL,
        .safety = PET_BEHAVIOR_SAFETY_NORMAL,
    };
    pet_state_t next_state = PET_STATE_LOCAL_REACT;

    s_pet.last_event = event;

    switch (event) {
    case PET_EVENT_IDLE:
        request.emotion = "smiling";
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_TOUCH_HEAD_SHORT:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_HAPPY_CHIRP();
        request.haptic = "tap";
        request.local_delta.affection = 1;
        request.local_delta.mood = 1;
        break;

    case PET_EVENT_TOUCH_HEAD_DOUBLE:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_DONE();
        request.local_delta.novelty = 1;
        break;

    case PET_EVENT_TOUCH_HEAD_LONG:
        request.action = PET_ACTION_SOOTHE;
        request.emotion = "caring";
        request.prompt_id = PET_PROMPT_ID_COMFORTED();
        request.haptic = "soft";
        request.local_delta.stress = -5;
        request.local_delta.affection = 2;
        next_state = PET_STATE_SOOTHE;
        break;

    case PET_EVENT_TOUCH_CHIN_SHORT:
        request.emotion = "curious";
        request.prompt_id = PET_PROMPT_ID_CURIOUS();
        request.haptic = "tap";
        request.local_delta.novelty = 1;
        break;

    case PET_EVENT_TOUCH_CHIN_DOUBLE:
        request.emotion = "winking";
        request.prompt_id = PET_PROMPT_ID_DONE();
        request.local_delta.mood = 1;
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

    case PET_EVENT_PRIVACY_OFF:
        s_pet.privacy = false;
        request.action = PET_ACTION_PRIVACY_OFF;
        request.emotion = "smiling";
        request.prompt_id = PET_PROMPT_ID_PRIVACY_OFF();
        request.haptic = "confirm";
        request.can_interrupt_tts = true;
        request.safety = PET_BEHAVIOR_SAFETY_PRIVACY;
        next_state = PET_STATE_IDLE_ALIVE;
        break;

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

    case PET_EVENT_AUDIO_LISTEN_STOP:
        request.emotion = "doubtful";
        request.prompt_id = PET_PROMPT_ID_THINKING();
        next_state = PET_STATE_CLOUD_THINK;
        break;

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

    case PET_EVENT_VISION_CAPTURE_START:
        request.emotion = "photo";
        request.prompt_id = PET_PROMPT_ID_PHOTO();
        next_state = PET_STATE_LOCAL_REACT;
        break;

    case PET_EVENT_VISION_CAPTURE_DONE:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_SUCCESS();
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_VISION_CAPTURE_ERROR:
        request.emotion = "confused";
        request.prompt_id = PET_PROMPT_ID_FAIL();
        next_state = PET_STATE_ERROR_DEGRADED;
        break;

    case PET_EVENT_SLEEP:
        request.action = PET_ACTION_SLEEP;
        request.emotion = "tired";
        request.prompt_id = PET_PROMPT_ID_SLEEP();
        next_state = PET_STATE_SLEEP;
        break;

    case PET_EVENT_WAKE:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_BOOT();
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_MOTION_PICKED_UP:
        request.emotion = "surprised";
        request.haptic = "tap";
        request.local_delta.novelty = 2;
        break;

    case PET_EVENT_MOTION_PUT_DOWN:
        request.emotion = "smiling";
        request.prompt_id = PET_PROMPT_ID_DONE();
        next_state = PET_STATE_IDLE_ALIVE;
        break;

    case PET_EVENT_MOTION_GENTLE_SHAKE:
        request.emotion = "happy";
        request.prompt_id = PET_PROMPT_ID_HAPPY_CHIRP();
        request.haptic = "confirm";
        request.local_delta.mood = 1;
        request.local_delta.novelty = 2;
        break;

    case PET_EVENT_MOTION_STRONG_SHAKE:
        request.emotion = "grimacing";
        request.prompt_id = PET_PROMPT_ID_IMPACT();
        request.haptic = "alert";
        request.local_delta.stress = 3;
        break;

    case PET_EVENT_MOTION_FREEFALL:
    case PET_EVENT_MOTION_IMPACT:
        request.action = PET_ACTION_SOOTHE;
        request.emotion = "afraid";
        request.prompt_id = PET_PROMPT_ID_AFRAID();
        request.haptic = "alert";
        request.can_interrupt_tts = true;
        request.safety = PET_BEHAVIOR_SAFETY_SAFETY;
        request.local_delta.stress = 5;
        request.local_delta.security = -5;
        next_state = PET_STATE_SOOTHE;
        break;

    case PET_EVENT_MOTION_TILT_LEFT:
    case PET_EVENT_MOTION_TILT_RIGHT:
        request.emotion = "curious";
        request.prompt_id = PET_PROMPT_ID_CURIOUS();
        request.local_delta.novelty = 1;
        break;

    default:
        LOGW("unknown pet event=%d\r\n", event);
        return BK_ERR_PARAM;
    }

    LOGI("event=%s next=%s action=%s emotion=%s privacy=%d\r\n",
         pet_brain_event_name(event),
         pet_brain_state_name(next_state),
         pet_action_router_action_name(request.action),
         request.emotion ? request.emotion : "",
         s_pet.privacy);
    return pet_brain_execute_behavior(next_state, &request);
}

void pet_brain_get_snapshot(pet_brain_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    *snapshot = s_pet;
}

const char *pet_brain_state_name(pet_state_t state)
{
    switch (state) {
    case PET_STATE_FACTORY:
        return "FACTORY";
    case PET_STATE_IDLE_ALIVE:
        return "IDLE_ALIVE";
    case PET_STATE_LOCAL_REACT:
        return "LOCAL_REACT";
    case PET_STATE_ATTENTION_CALL:
        return "ATTENTION_CALL";
    case PET_STATE_CLOUD_LISTEN:
        return "CLOUD_LISTEN";
    case PET_STATE_CLOUD_THINK:
        return "CLOUD_THINK";
    case PET_STATE_CLOUD_SPEAK:
        return "CLOUD_SPEAK";
    case PET_STATE_GAME:
        return "GAME";
    case PET_STATE_SOOTHE:
        return "SOOTHE";
    case PET_STATE_PRIVACY:
        return "PRIVACY";
    case PET_STATE_SLEEP:
        return "SLEEP";
    case PET_STATE_LOW_POWER:
        return "LOW_POWER";
    case PET_STATE_ERROR_DEGRADED:
        return "ERROR_DEGRADED";
    default:
        return "UNKNOWN";
    }
}

const char *pet_brain_event_name(pet_event_type_t event)
{
    for (uint32_t i = 0; i < sizeof(s_event_names) / sizeof(s_event_names[0]); i++) {
        if (s_event_names[i].event == event) {
            return s_event_names[i].name;
        }
    }

    return "unknown";
}

pet_event_type_t pet_brain_event_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return PET_EVENT_NONE;
    }

    for (uint32_t i = 0; i < sizeof(s_event_names) / sizeof(s_event_names[0]); i++) {
        if (os_strcasecmp(name, s_event_names[i].name) == 0) {
            return s_event_names[i].event;
        }
    }

    return PET_EVENT_NONE;
}
