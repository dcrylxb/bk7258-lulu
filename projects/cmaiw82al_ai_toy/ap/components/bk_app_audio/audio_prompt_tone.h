#ifndef __AUDIO_PROMPT_TONE_H__
#define __AUDIO_PROMPT_TONE_H__

#include <components/bk_audio/audio_pipeline/audio_element.h>
#include <components/bk_audio/audio_pipeline/audio_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *audio_prompt_tone_handle_t;

/**
 * @brief Audio prompt tone configuration structure
 */
typedef struct {
    uint8_t                 port_id;       /**< Audio prompt tone input port ID, must be >= 1 */
    uint32_t                rb_size;       /**< Player output ring buffer size */
    audio_dec_type_t        dec_type;      /**< Audio prompt tone decoder type */
    audio_element_handle_t  spk_stream;    /**< Speaker stream handle */
} audio_prompt_tone_cfg_t;

#define DEFAULT_AUDIO_PROMPT_TONE_CONFIG() {     \
    .port_id = 1,                               \
    .rb_size = 32*1024,                         \
    .dec_type = AUDIO_DEC_TYPE_MP3,             \
    .spk_stream = NULL,                         \
}

/**
 * @brief Audio prompt tone information structure
 */
typedef struct {
    player_uri_type_t uri_type;             /**< Audio prompt tone type, file or data */
    char              *data;                /**< Audio file path or data */
    uint32_t          len;                  /**< Length of the audio data */
} audio_prompt_tone_info_t;

/**
 * @brief      Initialize the audio prompt tone module
 *
 * @param[in]  cfg  The prompt tone configuration
 *
 * @return     The prompt tone handle on success, NULL on failure
 */
audio_prompt_tone_handle_t audio_prompt_tone_init(audio_prompt_tone_cfg_t *cfg);

/**
 * @brief      Start playing an audio prompt tone
 *
 * This function starts playing a prompt tone from an audio file or raw audio data.
 *
 * @param[in]  prompt_tone  The prompt tone handle
 * @param[in]  info         The prompt tone information
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t audio_prompt_tone_start(audio_prompt_tone_handle_t handle, audio_prompt_tone_info_t *info);

/**
 * @brief      Stop the audio prompt tone
 *
 * @param[in]  prompt_tone  The prompt tone handle
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t audio_prompt_tone_stop(audio_prompt_tone_handle_t handle);

/**
 * @brief      Deinitialize the audio prompt tone module
 *
 * @param[in]  prompt_tone  The prompt tone handle
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t audio_prompt_tone_deinit(audio_prompt_tone_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif