#ifndef __PET_PROMPT_H__
#define __PET_PROMPT_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PET_PROMPT_BASE_PATH "/if0/prompts"
#define PET_PROMPT_PATH_MAX  96

typedef enum {
    PET_PROMPT_RESULT_PLAYED = 0,
    PET_PROMPT_RESULT_SKIPPED,
    PET_PROMPT_RESULT_MISSING,
    PET_PROMPT_RESULT_ERROR,
} pet_prompt_result_t;

typedef enum {
    PET_PROMPT_SKIP_NONE = 0,
    PET_PROMPT_SKIP_EMPTY_ID,
    PET_PROMPT_SKIP_TTS_ACTIVE,
    PET_PROMPT_SKIP_MISSING,
    PET_PROMPT_SKIP_NOT_ALLOWED,
    PET_PROMPT_SKIP_THROTTLED,
} pet_prompt_skip_reason_t;

typedef struct {
    bool tts_active;
    bool can_interrupt_tts;
} pet_prompt_gate_t;

const char *PET_PROMPT_ID_BOOT(void);
const char *PET_PROMPT_ID_NET_OK(void);
const char *PET_PROMPT_ID_NET_LOST(void);
const char *PET_PROMPT_ID_LOW_POWER(void);
const char *PET_PROMPT_ID_ERROR(void);
const char *PET_PROMPT_ID_LISTEN_START(void);
const char *PET_PROMPT_ID_CANCEL(void);
const char *PET_PROMPT_ID_PHOTO(void);
const char *PET_PROMPT_ID_DONE(void);
const char *PET_PROMPT_ID_HAPPY_CHIRP(void);
const char *PET_PROMPT_ID_CURIOUS(void);
const char *PET_PROMPT_ID_AFRAID(void);
const char *PET_PROMPT_ID_COMFORTED(void);
const char *PET_PROMPT_ID_IMPACT(void);
const char *PET_PROMPT_ID_PRIVACY_ON(void);
const char *PET_PROMPT_ID_PRIVACY_OFF(void);
const char *PET_PROMPT_ID_STOP(void);
const char *PET_PROMPT_ID_SLEEP(void);
const char *PET_PROMPT_ID_THINKING(void);
const char *PET_PROMPT_ID_PROCESSING(void);
const char *PET_PROMPT_ID_SUCCESS(void);
const char *PET_PROMPT_ID_FAIL(void);
const char *PET_PROMPT_ID_WAKE_CONFIRM(void);
const char *PET_PROMPT_ID_TEST_WAKE_PROMPT(void);

bool pet_prompt_id_is_allowed(const char *prompt_id);
bk_err_t pet_prompt_path_for_id(const char *prompt_id, char *path, uint32_t path_len);
pet_prompt_result_t pet_prompt_play(const char *prompt_id,
                                    const pet_prompt_gate_t *gate,
                                    pet_prompt_skip_reason_t *skip_reason);
const char *pet_prompt_result_name(pet_prompt_result_t result);
const char *pet_prompt_skip_reason_name(pet_prompt_skip_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif
