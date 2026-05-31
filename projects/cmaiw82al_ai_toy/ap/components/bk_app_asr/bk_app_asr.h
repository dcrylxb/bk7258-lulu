#ifndef __BK_APP_ASR__H__
#define __BK_APP_ASR__H__

#include <common/bk_include.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[17];
    char key[33];
} asr_auth_t;

/**
 * @brief Initialize ASR module
 * 
 * @return int 0 - success, negative - failure
 */
int bk_app_asr_init(void);

/**
 * @brief Deinitialize ASR module
 */
void bk_app_asr_deinit(void);

/**
 * @brief Process ASR recognition
 * 
 * @param data Audio data buffer
 * @param len Length of audio data
 * @param p1 Parameter 1 for recognition result
 * @param p2 Parameter 2 for recognition result
 * @return int 0 - no result, 1 - has result, negative - error
 */
int bk_app_asr_recog(void *data, uint32_t len, void *p1, void *p2);

/**
 * @brief Set ASR authentication
 * 
 * @param auth ASR authentication structure
 * @return int 0 - success, negative - failure
 */
int bk_app_asr_set_auth(asr_auth_t *auth);

/**
 * @brief Get ASR authentication
 * 
 * @param auth ASR authentication structure
 * @return int 0 - success, negative - failure
 */
int bk_app_asr_get_auth(asr_auth_t *auth);

#ifdef __cplusplus
}
#endif

#endif // __BK_APP_ASR__H__
