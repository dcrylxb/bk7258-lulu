#include <components/log.h>
#include <components/bk_camera_ctlr.h>
#include <driver/gpio.h>
#include "gpio_driver.h"

#include "bk_app_camera.h"
#include "frame_que.h"

#define TAG "camera"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define GPIO_INVALID_ID   0xFF

typedef struct {
    bk_camera_ctlr_handle_t camera_hdl;
    image_format_t          format;
    uint8_t                 pwr_pin;
    bool                    is_init;
} camera_ctx_t;

static camera_ctx_t g_cam_ctx = {
    .camera_hdl = NULL,
    .format = IMAGE_MJPEG,
    .pwr_pin = GPIO_INVALID_ID,
};

static const bk_dvp_callback_t dvp_ctrl_cbs = {
    .malloc = frame_malloc,
    .complete = frame_complete,
};

static void _camera_power_on(uint8_t gpio)
{
    gpio_dev_unmap(gpio);
    bk_gpio_disable_input(gpio);
    bk_gpio_enable_output(gpio);
    bk_gpio_set_output_high(gpio);
}

static void _camera_power_off(uint8_t gpio)
{
    bk_gpio_set_output_low(gpio);
}

static void _dump_camera_config(app_camera_config_t *config)
{
    LOGI("=== Camera Configuration Parameters ===\r\n");
    LOGI("Image width      : %d pixels\r\n", config->width);
    LOGI("Image height     : %d pixels\r\n", config->height);
    LOGI("Image format     : %d\r\n", config->format);
    LOGI("Frame count      : %d\r\n", config->count);
    LOGI("Power pin        : %d\r\n", config->pwr_pin);
    LOGI("Reset pin        : %d\r\n", config->rst_pin);
    LOGI("======================================\r\n");
}

bk_err_t bk_app_camera_open(app_camera_config_t *config)
{
    avdk_err_t ret = AVDK_ERR_OK;
    bk_dvp_ctlr_config_t dvp_ctrl_config = {
        .config = BK_DVP_864X480_30FPS_MJPEG_CONFIG(),
        .cbs = &dvp_ctrl_cbs,
    };

    if (g_cam_ctx.is_init) {
        LOGE("camera is already opened\r\n");
        return BK_OK;
    }

    if (config == NULL) {
        LOGE("config is NULL\n");
        return BK_ERR_PARAM;
    }

    _dump_camera_config(config);

    do {
        g_cam_ctx.format = config->format;

        if (config->pwr_pin != GPIO_INVALID_ID) {
            LOGD("camera power on\r\n");
            g_cam_ctx.pwr_pin = config->pwr_pin;
            _camera_power_on(g_cam_ctx.pwr_pin);
        }

        if (frame_queue_init(config->count) != BK_OK) {
            LOGW("frame queue init fail\r\n");
        }

        dvp_ctrl_config.config.reset_pin = config->rst_pin;
        dvp_ctrl_config.config.img_format = config->format;
        dvp_ctrl_config.config.width = config->width;
        dvp_ctrl_config.config.height = config->height;
        dvp_ctrl_config.config.i2c_config.id = config->i2c_id;
        ret = bk_camera_dvp_ctlr_new(&g_cam_ctx.camera_hdl, &dvp_ctrl_config);
        if (ret != AVDK_ERR_OK) {
            LOGE("create camera ctlr fail, ret: %d\r\n", ret);
            break;
        }

        ret = bk_camera_open(g_cam_ctx.camera_hdl);
        if (ret != AVDK_ERR_OK) {
            LOGE("open camera fail, ret: %d\r\n", ret);
            break;
        }

        LOGI("open camera successful\r\n");

        g_cam_ctx.is_init = true;

        return BK_OK;
    } while (0);

    if (!g_cam_ctx.is_init) {
        if (g_cam_ctx.pwr_pin != GPIO_INVALID_ID) {
            _camera_power_off(g_cam_ctx.pwr_pin);
        }

        frame_queue_deinit();

        if (g_cam_ctx.camera_hdl != NULL) {
            bk_camera_delete(g_cam_ctx.camera_hdl);
            g_cam_ctx.camera_hdl = NULL;
        }            
    }

    return BK_FAIL;
}

bk_err_t bk_app_camera_close(void)
{
    if (!g_cam_ctx.is_init) {
        LOGE("camera is not opened\r\n");
        return BK_FAIL;
    }

    if (g_cam_ctx.camera_hdl != NULL) {
        bk_camera_close(g_cam_ctx.camera_hdl);
        bk_camera_delete(g_cam_ctx.camera_hdl);
        g_cam_ctx.camera_hdl = NULL;
    }

    frame_queue_deinit();

    if (g_cam_ctx.pwr_pin != GPIO_INVALID_ID) {
        LOGD("camera power off\r\n");
        _camera_power_off(g_cam_ctx.pwr_pin);
    }

    g_cam_ctx.is_init = false;

    LOGI("close camera successful\r\n");

    return BK_OK;
}

frame_buffer_t *bk_app_camera_get_frame(uint32_t timeout)
{
    if (!g_cam_ctx.is_init) {
        LOGE("camera is not opened\r\n");
        return NULL;
    }

    return frame_queue_get_frame(timeout);
}

void bk_app_camera_free_frame(frame_buffer_t *frame)
{
    if (frame == NULL) {
        LOGE("frame is NULL\r\n");
        return;
    }

    frame_free(g_cam_ctx.format, frame);
}