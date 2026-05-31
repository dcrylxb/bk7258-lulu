#ifndef __DIALOG_MODULE_H__
#define __DIALOG_MODULE_H__

#include "common.h"

#define PROMPT_NETWORK_PROVISION             "/if0/prompts/network_provision_16k_mono_16bit_zh.mp3"
#define PROMPT_NETWORK_PROVISION_FAIL        "/if0/prompts/network_provision_fail_16k_mono_16bit_zh.mp3"
#define PROMPT_NETWORK_PROVISION_SUCCESS     "/if0/prompts/network_provision_success_16k_mono_16bit_zh.mp3"
#define PROMPT_POWER_ON                      "/if0/prompts/power_on_16k_mono_16bit_zh.mp3"
#define PROMPT_SHUTDOWN                      "/if0/prompts/shutdown_16k_mono_16bit_zh.mp3"
#define PROMPT_FACTORY_RESET                 "/if0/prompts/factory_reset_16k_mono_16bit_zh.mp3"
#define PROMPT_NETWORK_CONNECTED             "/if0/prompts/network_connected_16k_mono_16bit_zh.mp3"
#define PROMPT_NETWORK_DISCONNECTED          "/if0/prompts/network_disconnected_16k_mono_16bit_zh.mp3"
#define PROMPT_DEVICE_NOT_ACTIVATED          "/if0/prompts/device_not_activated_16k_mono_16bit_zh.mp3"
#define PROMPT_DEVICE_ACTIVATED              "/if0/prompts/device_activated_16k_mono_16bit_zh.mp3"

typedef struct {
    uint32_t                     mic_cb_total;
    uint32_t                     mic_cb_recording;
    uint32_t                     mic_cb_forwarded;
    uint32_t                     mic_cb_dropped_status;
    uint32_t                     mic_cb_bytes;
    uint32_t                     mic_forwarded_bytes;
    int                          mic_last_status;
    uint32_t                     mic_last_len;
    uint32_t                     mic_last_cb_ms;
    uint32_t                     ws_audio_tx_count;
    uint32_t                     ws_audio_tx_bytes;
    uint32_t                     ws_audio_tx_fail;
    int                          ws_audio_last_len;
    int                          ws_audio_last_ret;
} dialog_voice_diag_t;

typedef struct {
    super_module_t               super;
    s32                          (*write_speaker_data)(u8 *speaker_data, uint32 speaker_data_len);
    void                         (*speaker_pa_control)(bool onff);
    s32                          (*speaker_play_abort)(void);
    void                         (*speaker_set_volume)(u8 volume);
    void                         (*speaker_play_prompt_tone)(char *path);

    bool                         m_is_init;
    int                          m_spk_volume;

    int                          m_mic_enc_type;
    bool                         m_dump_is_start;
    dialog_voice_diag_t          m_voice_diag;
    void                         (*dump_data_start)(void);
    void                         (*dump_data_stop)(void);
} dialog_module_t;

dialog_module_t *dialog_module_instance(void);
void dialog_voice_diag_get(dialog_voice_diag_t *diag);
void dialog_voice_diag_note_ws_audio_tx(int len, int ret);
#endif
