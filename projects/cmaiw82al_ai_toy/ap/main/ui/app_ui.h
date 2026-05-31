#ifndef __APP_UI_H__
#define __APP_UI_H__

#include <stdbool.h>
#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI module
 * 
 * This function initializes the UI module, including display, LVGL,
 * and other UI related components.
 * 
 * @return int Returns BK_OK on success, error code otherwise
 */
int app_ui_init(void);

/**
 * @brief Display WiFi connection status
 * 
 * This function displays the current WiFi connection status on the screen.
 * 
 * @param status WiFi connection status, true for connected, false for disconnected
 */
void app_ui_display_wifi_status(bool status);

/**
 * @brief Display chat message on screen
 * 
 * This function displays a text message on the chat interface.
 * 
 * @param text Pointer to the message text to be displayed
 */
void app_ui_display_chat_message(char *text);

/**
 * @brief Display chat emotion on screen
 * 
 * This function displays an emotion/emoji on the chat interface.
 * 
 * @param emo Pointer to the emotion identifier to be displayed
 */
void app_ui_display_chat_emotion(char *emo);

void app_ui_refresh_static_fallback(void);
bk_err_t app_ui_start_default_eye(void);
bk_err_t app_ui_start_eye_preheat(void);
bk_err_t app_ui_prepare_eye_resource_update(void);
bk_err_t app_ui_reopen_eye_resource(void);
bk_err_t app_ui_eye_debug_play(const char *file_name);
bk_err_t app_ui_eye_carousel_start(uint32_t dwell_seconds);
void app_ui_eye_carousel_stop(void);

#ifdef __cplusplus
}
#endif
#endif
