#ifndef __FRAME_QUE_H__
#define __FRAME_QUE_H__

#include <components/media_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize frame queue
 * 
 * @param count Number of frame buffers to initialize
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t frame_queue_init(uint32_t count);

/**
 * @brief Deinitialize frame queue
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t frame_queue_deinit(void);

/**
 * @brief Clear all frames in the queue
 *
 * @return BK_OK on success, error code on failure
 */
bk_err_t frame_queue_clear(void);

/**
 * @brief Get a frame from the queue
 *
 * @param timeout Timeout in milliseconds to wait for a frame
 *
 * @return Pointer to frame buffer on success, NULL on failure or timeout
 */
frame_buffer_t *frame_queue_get_frame(uint32_t timeout);

/**
 * @brief Free a frame buffer
 *
 * @param format  Image format of the frame
 * @param frame   Pointer to the frame buffer
 */
void frame_free(image_format_t format, frame_buffer_t *frame);

/**
 * @brief Allocate a frame buffer
 *
 * @param format  Image format of the frame
 * @param size    Size of the frame buffer in bytes
 *
 * @return Pointer to frame buffer on success, NULL on failure
 */
frame_buffer_t *frame_malloc(image_format_t format, uint32_t size);

/**
 * @brief Frame completion, push it to the queue
 *
 * @param format  Image format of the frame
 * @param frame   Pointer to the frame buffer
 */
void frame_complete(image_format_t format, frame_buffer_t *frame, int result);

#ifdef __cplusplus
}
#endif

#endif // __FRAME_QUE_H__