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
