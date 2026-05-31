#include <os/os.h>
#include <components/log.h>
#include "frame_buffer.h"

#include "frame_que.h"

#define TAG "frame_q"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct
{
    image_format_t format;
    uint32_t       frame;
} frame_msg_t;

static beken_queue_t frame_queue = NULL;

bk_err_t frame_queue_init(uint32_t count)
{
    bk_err_t ret = BK_OK;

    if (count == 0) {
        LOGE("frame queue count is 0\r\n");
        return BK_ERR_PARAM;
    }

    ret = rtos_init_queue(&frame_queue,
                          "frame_queue",
                          sizeof(frame_msg_t),
                          count);
    if (ret != BK_OK) {
        LOGE("create frame queue fail, ret: %d\r\n", ret);
        return ret;
    }

    return BK_OK;
}

bk_err_t frame_queue_deinit(void)
{
    frame_msg_t msg = {0};

    if (frame_queue == NULL) {
        LOGE("frame queue is not init\r\n");
        return BK_FAIL;
    }

    while (rtos_pop_from_queue(&frame_queue, &msg, 0) == BK_OK) {
        frame_buffer_t *frame = (frame_buffer_t *)msg.frame;
        frame_free(msg.format, frame);
    }

    rtos_deinit_queue(&frame_queue);

    return BK_OK;
}

bk_err_t frame_queue_clear(void)
{
    frame_msg_t msg = {0};

    if (frame_queue == NULL) {
        LOGE("frame queue is not init\r\n");
        return BK_FAIL;
    }

    while (rtos_pop_from_queue(&frame_queue, &msg, 0) == BK_OK) {
        frame_buffer_t *frame = (frame_buffer_t *)msg.frame;
        frame_free(msg.format, frame);
    }

    return BK_OK;
}

frame_buffer_t *frame_queue_get_frame(uint32_t timeout)
{
    frame_msg_t msg = {0};
    frame_buffer_t *frame = NULL;

    if (frame_queue == NULL) {
        LOGE("frame queue is not init\r\n");
        return NULL;
    }

    if (rtos_pop_from_queue(&frame_queue, &msg, timeout) != BK_OK) {
        LOGE("pop image frame from queue fail\r\n");
        return NULL;
    }

    frame = (frame_buffer_t *)msg.frame;
    return frame;
}

void frame_free(image_format_t format, frame_buffer_t *frame)
{
    if (frame == NULL) {
        return;
    }

    switch (format) {
    case IMAGE_MJPEG:
    case IMAGE_H264:
        frame_buffer_encode_free(frame);
        break;
    
    case IMAGE_YUV:
        frame_buffer_display_free(frame);
        break;
    
    default:
        LOGE("unknown image format: %d\r\n", format);
        break;
    }

    return;
}

frame_buffer_t *frame_malloc(image_format_t format, uint32_t size)
{
    frame_buffer_t *frame = NULL;

    switch (format) {
    case IMAGE_MJPEG:
    case IMAGE_H264:
        frame = frame_buffer_encode_malloc(size);
        break;
    
    case IMAGE_YUV:
        frame = frame_buffer_display_malloc(size);
        break;
    
    default:
        LOGE("unknown image format: %d\r\n", format);
        return NULL;
    }

    if (frame == NULL) {
        LOGE("malloc frame fail, format: %d, size: %d\r\n", format, size);
        return NULL;
    }

    return frame;
}

void frame_complete(image_format_t format, frame_buffer_t *frame, int result)
{
    bk_err_t ret = BK_OK;
    frame_msg_t msg = {0};

    if (result != 0) {
        frame_free(format, frame);
        return;
    }

    msg.format = format;
    msg.frame = (uint32_t)frame;

    ret = rtos_push_to_queue(&frame_queue, &msg, 0);
    if (ret != BK_OK) {
        frame_free(format, frame);
    }

    return;
}