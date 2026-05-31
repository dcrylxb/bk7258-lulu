#include <stdio.h>
#include <os/os.h>
#include <os/str.h>
#include <components/log.h>

#include "dialog_module.h"
#include "pet_prompt.h"

#define TAG "pet_prompt"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define PET_PROMPT_MIN_INTERVAL_MS 1200

typedef struct {
    const char *id;
} pet_prompt_entry_t;

const char *PET_PROMPT_ID_BOOT(void) { return "boot"; }
const char *PET_PROMPT_ID_NET_OK(void) { return "net_ok"; }
const char *PET_PROMPT_ID_NET_LOST(void) { return "net_lost"; }
const char *PET_PROMPT_ID_LOW_POWER(void) { return "low_power"; }
const char *PET_PROMPT_ID_ERROR(void) { return "error"; }
const char *PET_PROMPT_ID_LISTEN_START(void) { return "listen_start"; }
const char *PET_PROMPT_ID_CANCEL(void) { return "cancel"; }
const char *PET_PROMPT_ID_PHOTO(void) { return "photo"; }
const char *PET_PROMPT_ID_DONE(void) { return "done"; }
const char *PET_PROMPT_ID_HAPPY_CHIRP(void) { return "happy_chirp"; }
const char *PET_PROMPT_ID_CURIOUS(void) { return "curious"; }
const char *PET_PROMPT_ID_AFRAID(void) { return "afraid"; }
const char *PET_PROMPT_ID_COMFORTED(void) { return "comforted"; }
const char *PET_PROMPT_ID_IMPACT(void) { return "impact"; }
const char *PET_PROMPT_ID_PRIVACY_ON(void) { return "privacy_on"; }
const char *PET_PROMPT_ID_PRIVACY_OFF(void) { return "privacy_off"; }
const char *PET_PROMPT_ID_STOP(void) { return "stop"; }
const char *PET_PROMPT_ID_SLEEP(void) { return "sleep"; }
const char *PET_PROMPT_ID_THINKING(void) { return "thinking"; }
const char *PET_PROMPT_ID_PROCESSING(void) { return "processing"; }
const char *PET_PROMPT_ID_SUCCESS(void) { return "success"; }
const char *PET_PROMPT_ID_FAIL(void) { return "fail"; }
const char *PET_PROMPT_ID_WAKE_CONFIRM(void) { return "wake_confirm"; }
const char *PET_PROMPT_ID_TEST_WAKE_PROMPT(void) { return "test_wake_prompt"; }

static const pet_prompt_entry_t s_prompt_manifest[] = {
    {"boot"},
    {"net_ok"},
    {"net_lost"},
    {"low_power"},
    {"error"},
    {"listen_start"},
    {"cancel"},
    {"photo"},
    {"done"},
    {"happy_chirp"},
    {"curious"},
    {"afraid"},
    {"comforted"},
    {"impact"},
    {"privacy_on"},
    {"privacy_off"},
    {"stop"},
    {"sleep"},
    {"thinking"},
    {"processing"},
    {"success"},
    {"fail"},
    {"wake_confirm"},
    {"test_wake_prompt"},
};

static uint32_t s_last_prompt_play_ms = 0;

bool pet_prompt_id_is_allowed(const char *prompt_id)
{
    if (prompt_id == NULL || prompt_id[0] == '\0') {
        return false;
    }

    for (uint32_t i = 0; i < sizeof(s_prompt_manifest) / sizeof(s_prompt_manifest[0]); i++) {
        if (os_strcmp(prompt_id, s_prompt_manifest[i].id) == 0) {
            return true;
        }
    }

    return false;
}

bk_err_t pet_prompt_path_for_id(const char *prompt_id, char *path, uint32_t path_len)
{
    int written = 0;

    if (path == NULL || path_len == 0 || !pet_prompt_id_is_allowed(prompt_id)) {
        return BK_ERR_PARAM;
    }

    written = snprintf(path, path_len, "%s/%s.mp3", PET_PROMPT_BASE_PATH, prompt_id);
    if (written <= 0 || (uint32_t)written >= path_len) {
        return BK_ERR_NO_MEM;
    }

    return BK_OK;
}

pet_prompt_result_t pet_prompt_play(const char *prompt_id,
                                    const pet_prompt_gate_t *gate,
                                    pet_prompt_skip_reason_t *skip_reason)
{
    char path[PET_PROMPT_PATH_MAX] = {0};
    bk_err_t ret = BK_OK;
    uint32_t now_ms = rtos_get_time();

    if (skip_reason != NULL) {
        *skip_reason = PET_PROMPT_SKIP_NONE;
    }

    if (prompt_id == NULL || prompt_id[0] == '\0') {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_EMPTY_ID;
        }
        LOGI("skip id=(empty) reason=%s\r\n", pet_prompt_skip_reason_name(PET_PROMPT_SKIP_EMPTY_ID));
        return PET_PROMPT_RESULT_SKIPPED;
    }

    if (gate != NULL && gate->tts_active && !gate->can_interrupt_tts) {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_TTS_ACTIVE;
        }
        LOGI("skip id=%s reason=%s\r\n", prompt_id, pet_prompt_skip_reason_name(PET_PROMPT_SKIP_TTS_ACTIVE));
        return PET_PROMPT_RESULT_SKIPPED;
    }

    ret = pet_prompt_path_for_id(prompt_id, path, sizeof(path));
    if (ret != BK_OK) {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_NOT_ALLOWED;
        }
        LOGW("skip id=%s reason=%s ret=%d\r\n",
             prompt_id,
             pet_prompt_skip_reason_name(PET_PROMPT_SKIP_NOT_ALLOWED),
             ret);
        return PET_PROMPT_RESULT_SKIPPED;
    }

    if (s_last_prompt_play_ms != 0 &&
        (uint32_t)(now_ms - s_last_prompt_play_ms) < PET_PROMPT_MIN_INTERVAL_MS) {
        if (skip_reason != NULL) {
            *skip_reason = PET_PROMPT_SKIP_THROTTLED;
        }
        LOGI("skip id=%s reason=%s elapsed=%u min=%u\r\n",
             prompt_id,
             pet_prompt_skip_reason_name(PET_PROMPT_SKIP_THROTTLED),
             (uint32_t)(now_ms - s_last_prompt_play_ms),
             PET_PROMPT_MIN_INTERVAL_MS);
        return PET_PROMPT_RESULT_SKIPPED;
    }

    dialog_module_instance()->speaker_play_prompt_tone(path);
    s_last_prompt_play_ms = now_ms;
    LOGI("play id=%s path=%s\r\n", prompt_id, path);
    return PET_PROMPT_RESULT_PLAYED;
}

const char *pet_prompt_result_name(pet_prompt_result_t result)
{
    switch (result) {
    case PET_PROMPT_RESULT_PLAYED:
        return "played";
    case PET_PROMPT_RESULT_SKIPPED:
        return "skipped";
    case PET_PROMPT_RESULT_MISSING:
        return "missing";
    case PET_PROMPT_RESULT_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

const char *pet_prompt_skip_reason_name(pet_prompt_skip_reason_t reason)
{
    switch (reason) {
    case PET_PROMPT_SKIP_NONE:
        return "none";
    case PET_PROMPT_SKIP_EMPTY_ID:
        return "empty_id";
    case PET_PROMPT_SKIP_TTS_ACTIVE:
        return "tts_active";
    case PET_PROMPT_SKIP_MISSING:
        return "missing";
    case PET_PROMPT_SKIP_NOT_ALLOWED:
        return "not_allowed";
    case PET_PROMPT_SKIP_THROTTLED:
        return "throttled";
    default:
        return "unknown";
    }
}
