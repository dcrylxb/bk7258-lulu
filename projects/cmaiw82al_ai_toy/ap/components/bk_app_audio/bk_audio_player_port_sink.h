#ifndef __BK_AUDIO_PLAYER_PORT_SINK_H__
#define __BK_AUDIO_PLAYER_PORT_SINK_H__

#include <components/bk_audio/audio_pipeline/audio_element.h>
#include <components/bk_audio_player/bk_audio_player_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Port sink configuration structure
 *
 * This structure defines the configuration for the port sink,
 * which registers to a shared onboard_speaker_stream using multi-input port.
 * The ring buffer port must be created externally and passed in.
 */
typedef struct {
    audio_element_handle_t  spk_str;        /**< Shared onboard_speaker_stream handle (required) */
    audio_port_handle_t     rb_port;        /**< Ring buffer port handle (required) */
    uint8_t                 port_id;        /**< Input port ID for registration, must be >= 1 (required) */
    uint8_t                 priority;       /**< Port priority, higher value = higher priority (required) */
} bk_audio_player_port_sink_param_t;

/**
 * @brief Default port sink parameter configuration
 */
#define DEFAULT_PORT_SINK_PARAM() {             \
    .spk_str = NULL,                            \
    .rb_port = NULL,                            \
    .port_id = 2,                               \
    .priority = 0,                              \
}

/**
 * @brief Get port sink operations
 *
 * @return Pointer to sink operations structure
 */
const bk_audio_player_sink_ops_t *bk_audio_player_get_port_sink_ops(void);

/**
 * @brief Set port sink parameter
 *
 * @param param Pointer to port sink parameter structure
 * 
 * @return BK_OK on success, or error code on failure
 */
int bk_audio_player_set_port_sink_param(bk_audio_player_port_sink_param_t *param);

#ifdef __cplusplus
}
#endif
#endif /* __BK_AUDIO_PLAYER_PORT_SINK_H__ */
