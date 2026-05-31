#ifndef __BK_APP_CAMERA_H__
#define __BK_APP_CAMERA_H__

#include <components/media_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera configuration structure
 */
typedef struct {
    uint16_t            width;                          ///< Image width in pixels
    uint16_t            height;                         ///< Image height in 
    image_format_t      format;                         ///< Image format
    uint32_t            count;                          ///< Frame count
    uint8_t             i2c_id;                         ///< I2C bus ID
    uint8_t             pwr_pin;                        ///< Power control pin number
    uint8_t             rst_pin;                        ///< Reset pin number
} app_camera_config_t;

#define DEFAULT_APP_CAMERA_CONFIG() {             \
    .width         = 640,                         \
    .height        = 480,                         \
    .format        = IMAGE_MJPEG,                 \
    .count         = 5,                           \
    .pwr_pin       = 0xFF,                        \
    .rst_pin       = 0xFF,                        \
}

/**
 * @brief Open camera device
 *
 * @param config  Camera configuration parameters
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_camera_open(app_camera_config_t *config);

/**
 * @brief Close camera device
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t bk_app_camera_close(void);

/**
 * @brief Get a frame from camera
 *
 * @param timeout  Timeout in milliseconds to wait for a frame
 *
 * @return Pointer to frame buffer on success, NULL on failure or timeout
 */
frame_buffer_t *bk_app_camera_get_frame(uint32_t timeout);

/**
 * @brief Free a frame buffer
 *
 * @param frame  Pointer to frame buffer to be freed
 */
void bk_app_camera_free_frame(frame_buffer_t *frame);

#ifdef __cplusplus
}
#endif

#endif // __BK_APP_CAMERA_H__
