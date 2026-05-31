#ifndef __PET_HAPTIC_H__
#define __PET_HAPTIC_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PET_HAPTIC_DRIVER_MS32008N1_OUT5 1
#define PET_HAPTIC_ENABLE_MS32008N1_OUTPUT 1
#define PET_HAPTIC_MAX_DURATION_MS 3000
#define PET_HAPTIC_BUSINESS_MAX_DURATION_MS 120
#define PET_HAPTIC_DC_TEST_DEFAULT_MS 60
#define PET_HAPTIC_DC_TEST_MAX_MS 120
#define PET_HAPTIC_PATTERN_NAME_MAX 16

typedef struct {
    bool initialized;
    bool hardware_output_enabled;
    bool driver_awake;
    uint32_t nsleep_gpio;
    uint32_t i2c_id;
    uint8_t i2c_addr;
    char last_pattern[PET_HAPTIC_PATTERN_NAME_MAX];
    uint32_t last_duration_ms;
} pet_haptic_status_t;

typedef struct {
    uint8_t i2c_id;
    uint8_t i2c_addr;
    bool ack;
    bk_err_t ret;
    uint8_t chip_flag;
} pet_haptic_probe_result_t;

typedef struct {
    uint32_t requested_duration_ms;
    uint32_t actual_duration_ms;
    bool reverse;
    uint8_t chip_flag;
    bk_err_t ret;
} pet_haptic_dc_test_result_t;

bk_err_t pet_haptic_init(void);
bk_err_t pet_haptic_probe(pet_haptic_probe_result_t *result);
bk_err_t pet_haptic_dc_test(uint32_t duration_ms, bool reverse, pet_haptic_dc_test_result_t *result);
bk_err_t pet_haptic_play(const char *pattern, uint32_t duration_ms);
bk_err_t pet_haptic_stop(void);
void pet_haptic_get_status(pet_haptic_status_t *status);
bool pet_haptic_pattern_is_allowed(const char *pattern);
const char *pet_haptic_normalize_pattern(const char *pattern, const char *fallback);

#ifdef __cplusplus
}
#endif

#endif
