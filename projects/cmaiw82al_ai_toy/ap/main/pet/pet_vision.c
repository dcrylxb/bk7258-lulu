#include <os/mem.h>
#include <components/log.h>
#include <driver/gpio.h>

#include "common.h"
#include "pet_vision.h"

#if CONFIG_IOT_DEV_CAMERA
#include "bk_app_camera.h"
#endif

#define TAG "pet_vision"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

bk_err_t pet_vision_capture_once(pet_vision_capture_t *capture)
{
#if CONFIG_IOT_DEV_CAMERA
    app_camera_config_t cam_cfg = DEFAULT_APP_CAMERA_CONFIG();
    frame_buffer_t *frame = NULL;
    bk_err_t ret = BK_OK;

    if (capture == NULL) {
        return BK_ERR_PARAM;
    }

    os_memset(capture, 0, sizeof(*capture));
    cam_cfg.count = 1;
    cam_cfg.i2c_id = CAMERA_I2C_ID;
    cam_cfg.pwr_pin = CAMERA_PWR_GPIO;
    cam_cfg.rst_pin = CAMERA_RST_GPIO;

    ret = bk_app_camera_open(&cam_cfg);
    if (ret != BK_OK) {
        LOGE("camera open failed ret=%d\r\n", ret);
        return ret;
    }

    frame = bk_app_camera_get_frame(2000);
    if (frame == NULL) {
        LOGE("camera capture timeout\r\n");
        ret = BK_FAIL;
        goto out;
    }

    capture->width = frame->width;
    capture->height = frame->height;
    capture->size = frame->size;
    capture->length = frame->length;
    capture->sequence = frame->sequence;
    if (frame->frame != NULL && frame->length >= sizeof(capture->prefix)) {
        os_memcpy(capture->prefix, frame->frame, sizeof(capture->prefix));
        capture->jpeg_soi = (capture->prefix[0] == 0xff && capture->prefix[1] == 0xd8);
    }

    LOGI("capture width=%u height=%u size=%u length=%u sequence=%u jpeg_soi=%d prefix=%02x %02x %02x %02x\r\n",
         capture->width,
         capture->height,
         capture->size,
         capture->length,
         capture->sequence,
         capture->jpeg_soi,
         capture->prefix[0],
         capture->prefix[1],
         capture->prefix[2],
         capture->prefix[3]);

out:
    if (frame != NULL) {
        bk_app_camera_free_frame(frame);
    }
    (void)bk_app_camera_close();
    return ret;
#else
    (void)capture;
    return BK_ERR_NOT_SUPPORT;
#endif
}
