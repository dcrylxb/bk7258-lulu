#ifndef __BK_APP_AUDIO_PARAM_H__
#define __BK_APP_AUDIO_PARAM_H__

#include <components/audio_param_ctrl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Get custom audio parameters for specific service type
 *
 * This function retrieves custom audio parameters for the specified audio service type.
 * The returned structure contains system configuration, microphone configuration,
 * speaker configuration, EQ configuration, and AEC configuration for the service.
 *
 * @param[in]  service_type  The audio service type
 *                           - AUD_SERVICE_DOORBELL_VOC: Doorbell voice service
 *                           - AUD_SERVICE_ASR: Automatic Speech Recognition service
 *                           - AUD_SERVICE_AI_VOC: AI voice service
 *                           - AUD_SERVICE_SINGLE_MIC: Single microphone service
 *                           - AUD_SERVICE_SINGLE_SPK: Single speaker service
 *
 * @return     Pointer to the audio parameters structure on success, NULL on failure
 */
app_aud_para_t *bk_app_audio_get_cust_para(app_aud_service_type_t service_type);

/**
 * @brief      Initialize custom audio parameters for specific service type
 *
 * This function initializes custom audio parameters for the specified audio service type.
 * The voice_cfg parameter contains the necessary configuration for the audio service,
 * such as event handle, arguments, and other service-specific settings.
 *
 * @param[in]  service_type  The audio service type
 *                           - AUD_SERVICE_DOORBELL_VOC: Doorbell voice service
 *                           - AUD_SERVICE_ASR: Automatic Speech Recognition service
 *                           - AUD_SERVICE_AI_VOC: AI voice service
 *                           - AUD_SERVICE_SINGLE_MIC: Single microphone service
 *                           - AUD_SERVICE_SINGLE_SPK: Single speaker service
 */
void bk_app_audio_param_init(app_aud_service_type_t service_type);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __BK_APP_AUDIO_PARAM_H__