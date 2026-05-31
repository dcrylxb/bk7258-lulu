#ifndef __PET_ACTION_ROUTER_H__
#define __PET_ACTION_ROUTER_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PET_ACTION_MAX_DURATION_MS 15000
#define PET_ACTION_MAX_LOCAL_DELTA 5

typedef enum {
    PET_ACTION_NONE = 0,
    PET_ACTION_REPLY,
    PET_ACTION_EMOTE,
    PET_ACTION_SOOTHE,
    PET_ACTION_SLEEP,
    PET_ACTION_PRIVACY_ON,
    PET_ACTION_PRIVACY_OFF,
    PET_ACTION_ABORT,
} pet_action_type_t;

typedef struct {
    int8_t mood;
    int8_t energy;
    int8_t affection;
    int8_t security;
    int8_t novelty;
    int8_t stress;
} pet_local_delta_t;

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

const char *pet_action_router_normalize_emotion(const char *emotion, const char *fallback);
bool pet_action_router_emotion_is_allowed(const char *emotion);
pet_action_type_t pet_action_router_action_from_name(const char *name);
int8_t pet_action_router_clip_delta(int value);
uint32_t pet_action_router_clip_duration(uint32_t duration_ms);
bk_err_t pet_action_router_request_from_json(cJSON *root, pet_action_request_t *request);
bk_err_t pet_action_router_execute(const pet_action_request_t *request);
const char *pet_action_router_action_name(pet_action_type_t action);

#ifdef __cplusplus
}
#endif

#endif
