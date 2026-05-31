#include <components/log.h>
#include "lcd_panel_devices.h"
#include "gpio_driver.h"
#include <driver/gpio.h>
#include <driver/pwr_clk.h>
#include <driver/lcd.h>
#include "lvgl.h"
#include "lv_vendor.h"

#include "common.h"
#include "cmaiw82al_lcd.h"
#include "app_ui.h"
#include "display_module.h"

#define TAG "ui"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

extern lv_vnd_config_t vendor_config;

static bk_err_t lcd_backlight_open(uint8_t bl_io)
{
    gpio_dev_unmap(bl_io);
    BK_LOG_ON_ERR(bk_gpio_enable_output(bl_io));
    BK_LOG_ON_ERR(bk_gpio_pull_up(bl_io));
    bk_gpio_set_output_high(bl_io);

    return BK_OK;
}

static bk_err_t lcd_backlight_close(uint8_t bl_io)
{
    BK_LOG_ON_ERR(bk_gpio_pull_down(bl_io));
    bk_gpio_set_output_low(bl_io);

    return BK_OK;
}

static bool _display_is_ready(void)
{
    disp_module_t *module = display_module_instance();

    return module->m_is_init;
}

static int _display_ctrl_new(disp_lcd_config_t config, const lcd_device_t *device, bk_display_ctlr_handle_t *handle)
{
    bk_err_t ret = BK_FAIL;

    LOGD("new display ctrl, name: %s\r\n", config.name);

    if (device->type == LCD_TYPE_RGB565) {
        bk_display_rgb_ctlr_config_t rgb_ctlr_config;
        os_memset(&rgb_ctlr_config, 0, sizeof(bk_display_rgb_ctlr_config_t));
        rgb_ctlr_config.lcd_device = device;
        rgb_ctlr_config.clk_pin = config.config.rgb.clk_pin;
        rgb_ctlr_config.cs_pin = config.config.rgb.cs_pin;
        rgb_ctlr_config.sda_pin = config.config.rgb.sda_pin;
        rgb_ctlr_config.rst_pin = config.config.rgb.rst_pin;

        ret = bk_display_rgb_new(handle, &rgb_ctlr_config);
    } else if (device->type == LCD_TYPE_MCU8080) {
        bk_display_mcu_ctlr_config_t mcu_ctlr_config;
        os_memset(&mcu_ctlr_config, 0, sizeof(bk_display_mcu_ctlr_config_t));
        mcu_ctlr_config.lcd_device = device;

        ret = bk_display_mcu_new(handle, &mcu_ctlr_config);
    } else if (device->type == LCD_TYPE_SPI) {
        if (config.is_dual) {
            bk_display_dual_spi_ctlr_config_t dual_spi_ctlr_config;
            os_memset(&dual_spi_ctlr_config, 0, sizeof(bk_display_dual_spi_ctlr_config_t));

            dual_spi_ctlr_config.lcd0_config.lcd_device = device;
            dual_spi_ctlr_config.lcd0_config.spi_id = config.config.dual_spi.lcd0.spi_id;
            dual_spi_ctlr_config.lcd0_config.dc_pin = config.config.dual_spi.lcd0.dc_pin;
            dual_spi_ctlr_config.lcd0_config.reset_pin = config.config.dual_spi.lcd0.rst_pin;

            dual_spi_ctlr_config.lcd1_config.lcd_device = device;
            dual_spi_ctlr_config.lcd1_config.spi_id = config.config.dual_spi.lcd1.spi_id;
            dual_spi_ctlr_config.lcd1_config.dc_pin = config.config.dual_spi.lcd1.dc_pin;
            dual_spi_ctlr_config.lcd1_config.reset_pin = config.config.dual_spi.lcd1.rst_pin;
    
            ret = bk_display_dual_spi_new(handle, &dual_spi_ctlr_config);
        } else {
            bk_display_spi_ctlr_config_t spi_ctlr_config;
            os_memset(&spi_ctlr_config, 0, sizeof(bk_display_spi_ctlr_config_t));

            spi_ctlr_config.lcd_device = device;
            spi_ctlr_config.spi_id = config.config.spi.spi_id;
            spi_ctlr_config.dc_pin = config.config.spi.dc_pin;
            spi_ctlr_config.reset_pin = config.config.spi.rst_pin;
        
            ret = bk_display_spi_new(handle, &spi_ctlr_config);
        }
    } else if (device->type == LCD_TYPE_QSPI) {
        if (config.is_dual) {
            bk_display_dual_qspi_ctlr_config_t dual_qspi_ctlr_config;
            os_memset(&dual_qspi_ctlr_config, 0, sizeof(bk_display_dual_qspi_ctlr_config_t));

            dual_qspi_ctlr_config.lcd0_config.lcd_device = device;
            dual_qspi_ctlr_config.lcd0_config.qspi_id = config.config.dual_qspi.lcd0.qspi_id;
            dual_qspi_ctlr_config.lcd0_config.reset_pin = config.config.dual_qspi.lcd0.rst_pin;

            dual_qspi_ctlr_config.lcd1_config.lcd_device = device;
            dual_qspi_ctlr_config.lcd1_config.qspi_id = config.config.dual_qspi.lcd1.qspi_id;
            dual_qspi_ctlr_config.lcd1_config.reset_pin = config.config.dual_qspi.lcd1.rst_pin;
    
            ret = bk_display_dual_qspi_new(handle, &dual_qspi_ctlr_config);
        } else {
            bk_display_qspi_ctlr_config_t qspi_ctlr_config;
            os_memset(&qspi_ctlr_config, 0, sizeof(bk_display_qspi_ctlr_config_t));

            qspi_ctlr_config.lcd_device = device;
            qspi_ctlr_config.qspi_id = config.config.qspi.qspi_id;
            qspi_ctlr_config.reset_pin = config.config.qspi.rst_pin;
        
            ret = bk_display_qspi_new(handle, &qspi_ctlr_config);
        }
    } else {
        LOGE("lcd type %d not support", device->type);
        return BK_FAIL;
    }

    return ret;
}

static int _display_init(void)
{
    int ret = BK_OK;
    lv_vnd_config_t lv_vnd_config = {0};
    disp_lcd_config_t lcd_config = APP_LCD_CONFIG();
    disp_module_t *module = display_module_instance();

    if (module->m_is_init) {
        LOGW("display module is already init\r\n");
        return BK_OK;
    }

    cmaiw82al_lcd_devices_init();

    const lcd_device_t *lcd_device = get_lcd_device_by_name(lcd_config.name);
    if (lcd_device == NULL) {
        LOGE("lcd_device %s not found\r\n", lcd_config.name);
        return BK_FAIL;
    }

    // 初始化多媒体HEAP，frame_buffer_display_malloc() 需要从多媒体HEAP分配内存
    bk_pm_module_vote_psram_ctrl(PM_POWER_PSRAM_MODULE_NAME_LVGL_CODE_RUN, PM_POWER_MODULE_STATE_ON);

    lv_vnd_config.width = LCD_WIDTH;
    lv_vnd_config.height = LCD_HEIGHT;

    lv_vnd_config.render_mode = RENDER_FULL_MODE;
    lv_vnd_config.rotation = ROTATE_NONE;

    for (int i = 0; i < CONFIG_LVGL_FRAME_BUFFER_NUM; i++) {
        lv_vnd_config.frame_buffer[i] = frame_buffer_display_malloc(lv_vnd_config.width * lv_vnd_config.height * sizeof(bk_color_t));
        if (lv_vnd_config.frame_buffer[i] == NULL) {
            LOGE("lv_frame_buffer[%d] malloc failed\r\n", i);
            goto error;
        }
        os_memset(lv_vnd_config.frame_buffer[i]->frame, 0x00, lv_vnd_config.frame_buffer[i]->size);
    }

    ret = _display_ctrl_new(lcd_config, lcd_device, &lv_vnd_config.handle);
    if (ret != BK_OK) {
        LOGE("create display fail, ret: %d\r\n", ret);
        goto error;
    }

    ret = lv_vendor_init(&lv_vnd_config);
    if (ret != BK_OK) {
        LOGE("lv vendor init fail, ret: %d\r\n", ret);
        goto error;
    }

    ret = bk_display_open(lv_vnd_config.handle);
    if (ret != BK_OK) {
        LOGE("open display fail, ret: %d\r\n", ret);
        goto error;
    }

#if (CONFIG_TP)
    drv_tp_open(lv_vnd_config.width, lv_vnd_config.height, TP_MIRROR_NONE);
#endif

    lv_vendor_disp_lock();
    app_ui_init();
    lv_vendor_disp_unlock();

    lv_vendor_start();

    app_ui_refresh_static_fallback();
    lcd_backlight_open(LCD_BACKLIGHT_GPIO);

    ret = app_ui_start_default_eye();
    if (ret != BK_OK) {
        LOGE("default eye start failed, ret: %d\r\n", ret);
        app_ui_refresh_static_fallback();
    }

    ret = app_ui_start_eye_preheat();
    if (ret != BK_OK) {
        LOGW("eye preheat start failed, ret: %d\r\n", ret);
    }

    module->m_is_init = true;

    return BK_OK;

error:
    lv_vendor_deinit();

    if (lv_vnd_config.handle != NULL) {
        bk_display_close(lv_vnd_config.handle);
        bk_display_delete(lv_vnd_config.handle);
        lv_vnd_config.handle = NULL;
    }

    for (int i = 0; i < CONFIG_LVGL_FRAME_BUFFER_NUM; i++) {
        if (lv_vnd_config.frame_buffer[i] != NULL) {
            frame_buffer_display_free(lv_vnd_config.frame_buffer[i]);
            lv_vnd_config.frame_buffer[i] = NULL;
        }
    }

    return ret;
}

static int _display_deinit(void)
{
    disp_module_t *module = display_module_instance();

    lcd_backlight_close(LCD_BACKLIGHT_GPIO);

    bk_display_close(vendor_config.handle);

#if (CONFIG_TP)
    drv_tp_close();
#endif

    lv_vendor_stop();

    bk_display_delete(vendor_config.handle);

    for (int i = 0; i < CONFIG_LVGL_FRAME_BUFFER_NUM; i++) {
        if (vendor_config.frame_buffer[i] != NULL) {
            frame_buffer_display_free(vendor_config.frame_buffer[i]);
            vendor_config.frame_buffer[i] = NULL;
        }
    }

    lv_vendor_deinit();

    module->m_is_init = false;

    return BK_OK;
}

static disp_module_t g_disp = {
    .super.init     = _display_init,
    .super.deinit   = _display_deinit,
    .dispText       = app_ui_display_chat_message,
    .dispEmoji      = app_ui_display_chat_emotion,
    .dispWifi       = app_ui_display_wifi_status,
    .isDispReady    = _display_is_ready,

    .m_is_init    = false,
};

disp_module_t* display_module_instance(void)
{
    return &g_disp;
}
