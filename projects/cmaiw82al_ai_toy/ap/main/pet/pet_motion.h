#ifndef __PET_MOTION_H__
#define __PET_MOTION_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#include "pet_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_MOTION_EVENT_NONE = 0,
    PET_MOTION_EVENT_STABLE,
    PET_MOTION_EVENT_PICKED_UP,
    PET_MOTION_EVENT_PUT_DOWN,
    PET_MOTION_EVENT_GENTLE_SHAKE,
    PET_MOTION_EVENT_STRONG_SHAKE,
    PET_MOTION_EVENT_FREEFALL,
    PET_MOTION_EVENT_IMPACT,
    PET_MOTION_EVENT_TILT_LEFT,
    PET_MOTION_EVENT_TILT_RIGHT,
} pet_motion_event_t;

typedef struct {
    int16_t x_raw;
    int16_t y_raw;
    int16_t z_raw;
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
    uint32_t magnitude_mg;
    uint32_t timestamp_ms;
    uint8_t addr;
    const char *bus_name;
    const char *variant_name;
} pet_motion_sample_t;

typedef struct {
    bool ready;
    bool monitor_enabled;
    uint8_t addr;
    const char *bus_name;
    const char *variant_name;
    pet_motion_event_t last_event;
} pet_motion_status_t;

bk_err_t pet_motion_init(void);
bk_err_t pet_motion_probe(void);
bk_err_t pet_motion_read_sample(pet_motion_sample_t *sample);
pet_motion_event_t pet_motion_classify_sample(const pet_motion_sample_t *sample);
const char *pet_motion_event_name(pet_motion_event_t event);
pet_motion_event_t pet_motion_event_from_name(const char *name);
pet_event_type_t pet_motion_to_pet_event(pet_motion_event_t event);
bk_err_t pet_motion_route_event(pet_motion_event_t event);
bk_err_t pet_motion_set_monitor_enabled(bool enabled);
void pet_motion_get_status(pet_motion_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
