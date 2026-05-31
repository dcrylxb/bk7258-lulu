#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/log.h>

#include "pet_scene.h"

#define TAG "pet_scene"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    pet_scene_type_t scene;
    const char *name;
    pet_event_type_t event;
} pet_scene_entry_t;

static pet_scene_status_t s_scene = {0};

static const pet_scene_entry_t s_scene_entries[] = {
    {PET_SCENE_IDLE, "idle", PET_EVENT_IDLE},
    {PET_SCENE_TOUCH_PLAY, "touch_play", PET_EVENT_TOUCH_HEAD_SHORT},
    {PET_SCENE_SOOTHE, "soothe", PET_EVENT_TOUCH_HEAD_LONG},
    {PET_SCENE_CURIOUS, "curious", PET_EVENT_TOUCH_CHIN_SHORT},
    {PET_SCENE_VOICE_LISTEN, "voice_listen", PET_EVENT_AUDIO_LISTEN_START},
    {PET_SCENE_VOICE_THINK, "voice_think", PET_EVENT_AUDIO_LISTEN_STOP},
    {PET_SCENE_VOICE_SPEAK, "voice_speak", PET_EVENT_AUDIO_TTS_START},
    {PET_SCENE_VOICE_DONE, "voice_done", PET_EVENT_AUDIO_TTS_STOP},
    {PET_SCENE_VISION_CAPTURE, "vision_capture", PET_EVENT_VISION_CAPTURE_START},
    {PET_SCENE_VISION_DONE, "vision_done", PET_EVENT_VISION_CAPTURE_DONE},
    {PET_SCENE_VISION_ERROR, "vision_error", PET_EVENT_VISION_CAPTURE_ERROR},
    {PET_SCENE_PRIVACY_ON, "privacy_on", PET_EVENT_PRIVACY_ON},
    {PET_SCENE_PRIVACY_OFF, "privacy_off", PET_EVENT_PRIVACY_OFF},
    {PET_SCENE_ERROR_NET, "error_net", PET_EVENT_CLOUD_ERROR},
    {PET_SCENE_MOTION_GENTLE, "motion_gentle", PET_EVENT_MOTION_GENTLE_SHAKE},
    {PET_SCENE_MOTION_ALERT, "motion_alert", PET_EVENT_MOTION_STRONG_SHAKE},
};

static bool pet_scene_is_priority(pet_scene_type_t scene)
{
    return scene == PET_SCENE_IDLE ||
           scene == PET_SCENE_PRIVACY_ON ||
           scene == PET_SCENE_PRIVACY_OFF ||
           scene == PET_SCENE_MOTION_ALERT ||
           scene == PET_SCENE_VISION_DONE ||
           scene == PET_SCENE_VISION_ERROR;
}

static bool pet_scene_privacy_blocks(pet_scene_type_t scene)
{
    return scene == PET_SCENE_VOICE_LISTEN ||
           scene == PET_SCENE_VISION_CAPTURE;
}

static bool pet_scene_requires_online(pet_scene_type_t scene)
{
    return scene == PET_SCENE_VOICE_LISTEN ||
           scene == PET_SCENE_VISION_CAPTURE;
}

static pet_scene_type_t pet_scene_event_to_scene(pet_event_type_t event)
{
    switch (event) {
    case PET_EVENT_IDLE:
        return PET_SCENE_IDLE;
    case PET_EVENT_TOUCH_HEAD_SHORT:
    case PET_EVENT_TOUCH_HEAD_DOUBLE:
        return PET_SCENE_TOUCH_PLAY;
    case PET_EVENT_TOUCH_HEAD_LONG:
        return PET_SCENE_SOOTHE;
    case PET_EVENT_TOUCH_CHIN_SHORT:
    case PET_EVENT_TOUCH_CHIN_DOUBLE:
    case PET_EVENT_MOTION_PICKED_UP:
    case PET_EVENT_MOTION_PUT_DOWN:
    case PET_EVENT_MOTION_TILT_LEFT:
    case PET_EVENT_MOTION_TILT_RIGHT:
        return PET_SCENE_CURIOUS;
    case PET_EVENT_AUDIO_LISTEN_START:
        return PET_SCENE_VOICE_LISTEN;
    case PET_EVENT_AUDIO_LISTEN_STOP:
        return PET_SCENE_VOICE_THINK;
    case PET_EVENT_AUDIO_TTS_START:
        return PET_SCENE_VOICE_SPEAK;
    case PET_EVENT_AUDIO_TTS_STOP:
        return PET_SCENE_VOICE_DONE;
    case PET_EVENT_AUDIO_ABORT:
    case PET_EVENT_TOUCH_CHIN_LONG:
        return PET_SCENE_IDLE;
    case PET_EVENT_VISION_CAPTURE_START:
        return PET_SCENE_VISION_CAPTURE;
    case PET_EVENT_VISION_CAPTURE_DONE:
        return PET_SCENE_VISION_DONE;
    case PET_EVENT_VISION_CAPTURE_ERROR:
        return PET_SCENE_VISION_ERROR;
    case PET_EVENT_PRIVACY_ON:
    case PET_EVENT_TOUCH_CHIN_VERY_LONG:
        return PET_SCENE_PRIVACY_ON;
    case PET_EVENT_PRIVACY_OFF:
        return PET_SCENE_PRIVACY_OFF;
    case PET_EVENT_CLOUD_CONNECTED:
        return PET_SCENE_IDLE;
    case PET_EVENT_CLOUD_ERROR:
        return PET_SCENE_ERROR_NET;
    case PET_EVENT_MOTION_GENTLE_SHAKE:
        return PET_SCENE_MOTION_GENTLE;
    case PET_EVENT_MOTION_STRONG_SHAKE:
    case PET_EVENT_MOTION_FREEFALL:
    case PET_EVENT_MOTION_IMPACT:
        return PET_SCENE_MOTION_ALERT;
    case PET_EVENT_SLEEP:
    case PET_EVENT_WAKE:
        return PET_SCENE_IDLE;
    default:
        return PET_SCENE_NONE;
    }
}

static pet_event_type_t pet_scene_default_event(pet_scene_type_t scene)
{
    for (uint32_t i = 0; i < sizeof(s_scene_entries) / sizeof(s_scene_entries[0]); i++) {
        if (s_scene_entries[i].scene == scene) {
            return s_scene_entries[i].event;
        }
    }

    return PET_EVENT_NONE;
}

static bk_err_t pet_scene_reject(pet_scene_type_t scene, pet_event_type_t event, pet_scene_reject_reason_t reason)
{
    s_scene.rejected_count++;
    s_scene.last_reject_reason = reason;
    LOGW("reject scene=%s event=%s reason=%s rejected=%u\r\n",
         pet_scene_type_name(scene),
         pet_brain_event_name(event),
         pet_scene_reject_reason_name(reason),
         s_scene.rejected_count);
    return BK_FAIL;
}

bk_err_t pet_scene_init(void)
{
    os_memset(&s_scene, 0, sizeof(s_scene));
    s_scene.current_scene = PET_SCENE_IDLE;
    s_scene.last_scene = PET_SCENE_NONE;
    LOGI("scene runtime init\r\n");
    return BK_OK;
}

bk_err_t pet_scene_handle_event(pet_event_type_t event)
{
    pet_scene_type_t scene = pet_scene_event_to_scene(event);
    uint32_t now_ms = rtos_get_time();
    bk_err_t ret = BK_OK;

    if (scene == PET_SCENE_NONE || event == PET_EVENT_NONE) {
        return pet_scene_reject(scene, event, PET_SCENE_REJECT_UNKNOWN);
    }

    if (s_scene.privacy_active && pet_scene_privacy_blocks(scene)) {
        return pet_scene_reject(scene, event, PET_SCENE_REJECT_PRIVACY);
    }

    if (!s_scene.cloud_online && pet_scene_requires_online(scene)) {
        return pet_scene_reject(scene, event, PET_SCENE_REJECT_OFFLINE);
    }

    if (scene == PET_SCENE_VISION_CAPTURE && s_scene.vision_busy) {
        return pet_scene_reject(scene, event, PET_SCENE_REJECT_VISION_BUSY);
    }

    if (!pet_scene_is_priority(scene) &&
        s_scene.current_scene == scene &&
        s_scene.last_enter_ms != 0 &&
        (uint32_t)(now_ms - s_scene.last_enter_ms) < PET_SCENE_REPEAT_COOLDOWN_MS) {
        return pet_scene_reject(scene, event, PET_SCENE_REJECT_REPEAT);
    }

    ret = pet_brain_handle_event(event);
    if (ret != BK_OK) {
        LOGW("brain rejected scene=%s event=%s ret=%d\r\n",
             pet_scene_type_name(scene),
             pet_brain_event_name(event),
             ret);
        return ret;
    }

    s_scene.last_scene = s_scene.current_scene;
    s_scene.current_scene = scene;
    s_scene.last_event = event;
    s_scene.last_enter_ms = now_ms;
    s_scene.last_reject_reason = PET_SCENE_REJECT_NONE;

    if (scene == PET_SCENE_PRIVACY_ON) {
        s_scene.privacy_active = true;
    } else if (scene == PET_SCENE_PRIVACY_OFF) {
        s_scene.privacy_active = false;
    }

    if (event == PET_EVENT_CLOUD_CONNECTED) {
        s_scene.cloud_online = true;
    } else if (event == PET_EVENT_CLOUD_ERROR) {
        s_scene.cloud_online = false;
        s_scene.voice_speaking = false;
        s_scene.vision_busy = false;
    }

    if (scene == PET_SCENE_VOICE_SPEAK) {
        s_scene.voice_speaking = true;
    } else if (scene == PET_SCENE_VOICE_DONE || scene == PET_SCENE_IDLE) {
        s_scene.voice_speaking = false;
    }

    if (scene == PET_SCENE_VISION_CAPTURE) {
        s_scene.vision_busy = true;
    } else if (scene == PET_SCENE_VISION_DONE || scene == PET_SCENE_VISION_ERROR) {
        s_scene.vision_busy = false;
    }

    LOGI("enter scene=%s event=%s online=%d voice=%d vision=%d privacy=%d\r\n",
         pet_scene_type_name(scene),
         pet_brain_event_name(event),
         s_scene.cloud_online,
         s_scene.voice_speaking,
         s_scene.vision_busy,
         s_scene.privacy_active);
    return BK_OK;
}

bk_err_t pet_scene_enter(pet_scene_type_t scene)
{
    pet_event_type_t event = pet_scene_default_event(scene);

    if (event == PET_EVENT_NONE) {
        return pet_scene_reject(scene, PET_EVENT_NONE, PET_SCENE_REJECT_UNKNOWN);
    }

    return pet_scene_handle_event(event);
}

void pet_scene_get_status(pet_scene_status_t *status)
{
    if (status == NULL) {
        return;
    }

    *status = s_scene;
}

pet_scene_type_t pet_scene_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return PET_SCENE_NONE;
    }

    for (uint32_t i = 0; i < sizeof(s_scene_entries) / sizeof(s_scene_entries[0]); i++) {
        if (os_strcasecmp(name, s_scene_entries[i].name) == 0) {
            return s_scene_entries[i].scene;
        }
    }

    return PET_SCENE_NONE;
}

const char *pet_scene_type_name(pet_scene_type_t scene)
{
    for (uint32_t i = 0; i < sizeof(s_scene_entries) / sizeof(s_scene_entries[0]); i++) {
        if (s_scene_entries[i].scene == scene) {
            return s_scene_entries[i].name;
        }
    }

    return "none";
}

const char *pet_scene_reject_reason_name(pet_scene_reject_reason_t reason)
{
    switch (reason) {
    case PET_SCENE_REJECT_NONE:
        return "none";
    case PET_SCENE_REJECT_UNKNOWN:
        return "unknown";
    case PET_SCENE_REJECT_PRIVACY:
        return "privacy";
    case PET_SCENE_REJECT_OFFLINE:
        return "offline";
    case PET_SCENE_REJECT_VISION_BUSY:
        return "vision_busy";
    case PET_SCENE_REJECT_REPEAT:
        return "repeat";
    default:
        return "unknown";
    }
}
