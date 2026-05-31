#ifndef __APP_AUDIO_PLAYER_H__
#define __APP_AUDIO_PLAYER_H__

#include <common/bk_include.h>
#include <components/bk_audio_player/bk_audio_player_types.h>
#include <components/bk_audio/audio_pipeline/audio_element.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief App audio player configuration structure
 */
typedef struct {
    uint8_t                 port_id;       /**< Player input port ID, must be >= 1 */
    audio_port_handle_t     rb_port;       /**< Player output ring buffer port handle */
    audio_element_handle_t  spk_stream;    /**< Speaker stream handle */
    uint8_t                 priority;      /**< Playback priority */
    audio_player_event_handler_func  event_handler; /**< Event handler callback function */
    void                            *user_data;     /**< User data for the event handler function */
} app_audio_player_cfg_t;

#define DEFAULT_APP_AUDIO_PLAYER_CONFIG() {         \
    .port_id = 2,                                   \
    .rb_port = NULL,                                \
    .spk_stream = NULL,                             \
    .priority = 2,                                  \
    .event_handler = NULL,                          \
    .user_data = NULL,                              \
}

/**
 * @brief      Initialize the app audio player module
 *
 * @param[in]  cfg  The player configuration
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_init(app_audio_player_cfg_t *cfg);

/**
 * @brief      Deinitialize the app audio player module
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_deinit(void);

/**
 * @brief      Add music to the playlist
 *
 * @param[in]  name  Music name (identifier)
 * @param[in]  uri   Music file path or URL
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_add_music(char *name, char *uri);

/**
 * @brief      Clear all music from the playlist
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_clear_music_list(void);

/**
 * @brief      Start playback
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_start(void);

/**
 * @brief      Stop playback
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_stop(void);

/**
 * @brief      Pause playback
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_pause(void);

/**
 * @brief      Resume playback
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_resume(void);

/**
 * @brief      Play next song in the playlist
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_next(void);

/**
 * @brief      Play previous song in the playlist
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_prev(void);

/**
 * @brief      Seek to a specific position in the current song
 *
 * @param[in]  second  Target position in seconds
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_seek(int second);

/**
 * @brief      Set playback volume
 *
 * @param[in]  volume  Volume value (0-100)
 *
 * @return     BK_OK on success, error code on failure
 */
bk_err_t app_audio_player_set_volume(int volume);

/**
 * @brief      Get current playback volume
 *
 * @return     Current volume value (0-100)
 */
int app_audio_player_get_volume(void);

#ifdef __cplusplus
}
#endif
#endif
