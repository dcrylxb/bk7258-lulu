#ifndef __APP_VOICE_H__
#define __APP_VOICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <common/bk_include.h>
#include <components/bk_voice_service_types.h>
#include <components/bk_player_service_types.h>

#if CONFIG_AUDIO_PLAYER
#include <components/bk_audio_player/bk_audio_player_types.h>
#endif

typedef enum {
    AUDIO_SPK_PORT_TTS    = 0,
    AUDIO_SPK_PORT_TONE   = 1,
    AUDIO_SPK_PORT_PLAYER = 2,
    AUDIO_SPK_PORT_A2DP   = 3,
    AUDIO_SPK_PORT_MAX
} AUDIO_SPK_PORT_ID_E;

typedef enum {
    AUDIO_SPK_PORT_PRIORITY_PLAYER = 1,
    AUDIO_SPK_PORT_PRIORITY_A2DP   = 2,
    AUDIO_SPK_PORT_PRIORITY_TONE   = 3,
    AUDIO_SPK_PORT_PRIORITY_MAX
} AUDIO_SPK_PORT_PRIORITY_E;

/**
 * @brief MIC data receive callback function type
 *
 * @param data  Received audio data
 * @param len   Data length
 * @param args  User arguments
 *
 * @return 0 for success, other values for error
 */
typedef int (*voice_mic_rx_cb_t)(unsigned char *data, unsigned int len, void *args);

/**
 * @brief ASR result callback function type
 *
 * @param param  ASR result pointer address
 */
typedef void (*voice_asr_result_cb_t)(uint32_t param);

/**
 * @brief Audio configuration structure
 */
typedef struct {
    bool                aec_enable;                     ///< AEC enable flag
    aec_v3_mode_t       aec_mode;                       ///< AEC mode
    bool                vad_enable;                     ///< VAD enable flag, dependent on AEC
    audio_dec_type_t    dec_type;                       ///< Decoder type
    uint32_t            spk_sample_rate;                ///< Speaker sample rate
    uint8_t             spk_frame_duration;             ///< Speaker frame duration in ms
    uint16_t            pa_ctrl_gpio;                   ///< GPIO id of control pa
    uint8_t             pa_on_level;                    ///< GPIO level of turn on pa, 0: low level, 1: high level
    bool                prompt_tone_enable;             ///< Prompt tone enable flag
    audio_dec_type_t    prompt_tone_dec_type;           ///< Prompt tone decoder type
    audio_enc_type_t    enc_type;                       ///< Encoder type
    uint32_t            mic_sample_rate;                ///< MIC sample rate
    voice_mic_rx_cb_t   mic_rx_cb;                      ///< MIC data receive callback function
    uint32_t            asr_frame_size;                 ///< ASR frame size in bytes
    voice_asr_result_cb_t asr_result_cb;                ///< ASR result callback function
    voice_event_handle  event_cb;                       ///< Voice event callback function
#if CONFIG_VAD
    vad_state_callback  vad_state_cb;                   ///< VAD state update callback function
#endif
#if CONFIG_AUDIO_PLAYER
    bool                audio_player_enable;            ///< Enable audio player
    audio_player_event_handler_func  audio_player_event_cb;  /**< App audio player event callback */
#endif
    void *              user_data;                      ///< User data for callback
} app_audio_config_t;

#define DEFAULT_APP_AUDIO_CONFIG() {                    \
    .aec_enable         = true,                         \
    .aec_mode           = AEC_MODE_HARDWARE,            \
    .vad_enable         = false,                        \
    .dec_type           = AUDIO_DEC_TYPE_OPUS,          \
    .spk_sample_rate    = 16000,                        \
    .spk_frame_duration = 60,                           \
    .pa_ctrl_gpio       = 0xFFFF,                       \
    .pa_on_level        = 1,                            \
    .enc_type           = AUDIO_ENC_TYPE_OPUS,          \
    .mic_sample_rate    = 16000,                        \
    .mic_rx_cb          = NULL,                         \
    .asr_result_cb      = NULL,                         \
}

/**
 * @brief Initialize audio service
 *
 * @param cfg  Audio configuration parameters
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_init(app_audio_config_t *cfg);

/**
 * @brief Deinitialize audio service
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_deinit(void);

/**
 * @brief Ensure voice pipeline, read task, and write task are running.
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_ensure_voice_streaming(void);

/**
 * @brief Recover voice streaming. Force restart stops and restarts the voice pipeline first.
 *
 * @param force_restart  true to stop/start the full voice pipeline before starting read/write tasks
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_recover_voice_streaming(bool force_restart);

/**
 * @brief Stop the prompt tone player and release its current file stream.
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_stop_prompt_tone(void);

/**
 * @brief Write audio data to voice service
 *
 * @param data  Audio data buffer
 * @param len   Length of audio data in bytes
 *
 * @return Number of bytes written, or negative value on error
 */
int bk_app_audio_write_data(uint8_t *data, uint32_t len);

/**
 * @brief Set speaker volume
 *
 * @param volume  Volume value (0-100)
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_set_volume(uint8_t volume);

/**
 * @brief Control PA (Power Amplifier) on/off
 *
 * @param enable  true to turn on PA, false to turn off PA
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_pa_control(bool enable);

/**
 * @brief Play prompt tone
 *
 * @param uri_type  Audio prompt tone type, file or data
 * @param data      Audio file path or data
 * @param len       Length of the audio data
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_audio_play_prompt_tone(player_uri_type_t uri_type, char *data, uint32_t len);

/**
 * @brief Get speaker element handle
 *
 * @return Speaker element handle
 */
audio_element_handle_t bk_app_audio_get_spk_element(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_VOICE_H__
