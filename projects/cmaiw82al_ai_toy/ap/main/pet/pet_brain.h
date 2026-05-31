#ifndef __PET_BRAIN_H__
#define __PET_BRAIN_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#include "pet_action_router.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_STATE_FACTORY = 0,
    PET_STATE_IDLE_ALIVE,
    PET_STATE_LOCAL_REACT,
    PET_STATE_ATTENTION_CALL,
    PET_STATE_CLOUD_LISTEN,
    PET_STATE_CLOUD_THINK,
    PET_STATE_CLOUD_SPEAK,
    PET_STATE_GAME,
    PET_STATE_SOOTHE,
    PET_STATE_PRIVACY,
    PET_STATE_SLEEP,
    PET_STATE_LOW_POWER,
    PET_STATE_ERROR_DEGRADED,
} pet_state_t;

typedef enum {
    PET_EVENT_NONE = 0,
    PET_EVENT_IDLE,
    PET_EVENT_TOUCH_HEAD_SHORT,
    PET_EVENT_TOUCH_HEAD_DOUBLE,
    PET_EVENT_TOUCH_HEAD_LONG,
    PET_EVENT_TOUCH_CHIN_SHORT,
    PET_EVENT_TOUCH_CHIN_DOUBLE,
    PET_EVENT_TOUCH_CHIN_LONG,
    PET_EVENT_TOUCH_CHIN_VERY_LONG,
    PET_EVENT_AUDIO_LISTEN_START,
    PET_EVENT_AUDIO_LISTEN_STOP,
    PET_EVENT_AUDIO_TTS_START,
    PET_EVENT_AUDIO_TTS_STOP,
    PET_EVENT_AUDIO_ABORT,
    PET_EVENT_CLOUD_CONNECTED,
    PET_EVENT_CLOUD_ERROR,
    PET_EVENT_VISION_CAPTURE_START,
    PET_EVENT_VISION_CAPTURE_DONE,
    PET_EVENT_VISION_CAPTURE_ERROR,
    PET_EVENT_PRIVACY_ON,
    PET_EVENT_PRIVACY_OFF,
    PET_EVENT_SLEEP,
    PET_EVENT_WAKE,
    PET_EVENT_MOTION_PICKED_UP,
    PET_EVENT_MOTION_PUT_DOWN,
    PET_EVENT_MOTION_GENTLE_SHAKE,
    PET_EVENT_MOTION_STRONG_SHAKE,
    PET_EVENT_MOTION_FREEFALL,
    PET_EVENT_MOTION_IMPACT,
    PET_EVENT_MOTION_TILT_LEFT,
    PET_EVENT_MOTION_TILT_RIGHT,
} pet_event_type_t;

typedef struct {
    pet_state_t state;
    bool privacy;
    int8_t mood;
    int8_t energy;
    int8_t affection;
    int8_t security;
    int8_t novelty;
    int8_t stress;
    pet_event_type_t last_event;
    pet_action_type_t last_action;
    char last_emotion[16];
} pet_brain_snapshot_t;

bk_err_t pet_brain_init(void);
bk_err_t pet_brain_handle_event(pet_event_type_t event);
bk_err_t pet_brain_emit_emotion(const char *emotion);
bk_err_t pet_brain_apply_action_request(const pet_action_request_t *request);
bk_err_t pet_brain_set_privacy(bool enabled);
void pet_brain_get_snapshot(pet_brain_snapshot_t *snapshot);
const char *pet_brain_state_name(pet_state_t state);
const char *pet_brain_event_name(pet_event_type_t event);
pet_event_type_t pet_brain_event_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif
