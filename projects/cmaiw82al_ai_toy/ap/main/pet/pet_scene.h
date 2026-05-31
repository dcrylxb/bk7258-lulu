#ifndef __PET_SCENE_H__
#define __PET_SCENE_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#include "pet_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PET_SCENE_REPEAT_COOLDOWN_MS 800

typedef enum {
    PET_SCENE_NONE = 0,
    PET_SCENE_IDLE,
    PET_SCENE_TOUCH_PLAY,
    PET_SCENE_SOOTHE,
    PET_SCENE_CURIOUS,
    PET_SCENE_VOICE_LISTEN,
    PET_SCENE_VOICE_THINK,
    PET_SCENE_VOICE_SPEAK,
    PET_SCENE_VOICE_DONE,
    PET_SCENE_VISION_CAPTURE,
    PET_SCENE_VISION_DONE,
    PET_SCENE_VISION_ERROR,
    PET_SCENE_PRIVACY_ON,
    PET_SCENE_PRIVACY_OFF,
    PET_SCENE_ERROR_NET,
    PET_SCENE_MOTION_GENTLE,
    PET_SCENE_MOTION_ALERT,
} pet_scene_type_t;

typedef enum {
    PET_SCENE_REJECT_NONE = 0,
    PET_SCENE_REJECT_UNKNOWN,
    PET_SCENE_REJECT_PRIVACY,
    PET_SCENE_REJECT_OFFLINE,
    PET_SCENE_REJECT_VISION_BUSY,
    PET_SCENE_REJECT_REPEAT,
} pet_scene_reject_reason_t;

typedef struct {
    pet_scene_type_t current_scene;
    pet_scene_type_t last_scene;
    pet_event_type_t last_event;
    uint32_t last_enter_ms;
    uint32_t rejected_count;
    pet_scene_reject_reason_t last_reject_reason;
    bool cloud_online;
    bool voice_speaking;
    bool vision_busy;
    bool privacy_active;
} pet_scene_status_t;

bk_err_t pet_scene_init(void);
bk_err_t pet_scene_handle_event(pet_event_type_t event);
bk_err_t pet_scene_enter(pet_scene_type_t scene);
void pet_scene_get_status(pet_scene_status_t *status);
pet_scene_type_t pet_scene_from_name(const char *name);
const char *pet_scene_type_name(pet_scene_type_t scene);
const char *pet_scene_reject_reason_name(pet_scene_reject_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif
