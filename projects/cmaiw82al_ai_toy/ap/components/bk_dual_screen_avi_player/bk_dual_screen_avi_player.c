#include "os/os.h"
#include "os/str.h"
#include "lcd_panel_devices.h"
#include "components/bk_display.h"
#include "lcd_panel_devices.h"
#include "avi_player.h"
#include "gpio_driver.h"
#include <driver/gpio.h>
#include "bk_posix.h"
#include "driver/flash_partition.h"
#include "common.h"
#include "cmaiw82al_lcd.h"


#define TAG "dual_avi"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)
#define LOGV(...) BK_LOGV(TAG, ##__VA_ARGS__)


#define LCD_BL_IO GPIO_25

#define FAIL_COUNT_MAX 3

static uint32_t g_dual_screen_avi_fail_cnt = 0;
static beken_thread_t g_dual_screen_avi_player_thread;
static beken_semaphore_t g_dual_screen_avi_player_sem;
static bool g_dual_screen_avi_player_is_running = false;
static bk_avi_player_t *handle = NULL;
static bk_avi_player_config_t g_avi_player_config = {0};
static bk_display_ctlr_handle_t lcd_display_handle = NULL;
static frame_buffer_t *lcd_frame_buffer = NULL;


bk_display_dual_spi_ctlr_config_t dual_spi_ctlr_config = {
    .lcd0_config.lcd_device = &lcd_device_cmaiw82al_gc9d01_160,
    .lcd0_config.spi_id = 0,
    .lcd0_config.dc_pin = GPIO_7,
    .lcd0_config.reset_pin = LCD2_RESET_GPIO,
    .lcd0_config.te_pin = 0,
    .lcd1_config.lcd_device = &lcd_device_cmaiw82al_gc9d01_160,
    .lcd1_config.spi_id = 1,
    .lcd1_config.dc_pin = GPIO_5,
    .lcd1_config.reset_pin = LCD1_RESET_GPIO,
    .lcd1_config.te_pin = 0,
};


static avdk_err_t lcd_backlight_open(uint8_t bl_io)
{
    gpio_dev_unmap(bl_io);
    BK_LOG_ON_ERR(bk_gpio_enable_output(bl_io));
    BK_LOG_ON_ERR(bk_gpio_pull_up(bl_io));
    bk_gpio_set_output_high(bl_io);

    return AVDK_ERR_OK;
}

static avdk_err_t lcd_backlight_close(uint8_t bl_io)
{
    BK_LOG_ON_ERR(bk_gpio_pull_down(bl_io));
    bk_gpio_set_output_low(bl_io);

    return AVDK_ERR_OK;
}

static int _fs_mount(void)
{
    /* app_vfs_init() owns /sf0 mounting. Do not remount SD NAND here. */
    return BK_OK;
}

static bk_err_t bk_avi_player_vfs_init(void)
{
    bk_err_t ret = BK_FAIL;

    do {
        ret = _fs_mount();
        if (BK_OK != ret)
        {
            BK_LOGD(NULL, "[%s][%d] mount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
            break;
        }

        BK_LOGD(NULL, "[%s][%d] mount success\r\n", __FUNCTION__, __LINE__);
    } while(0);

    return ret;
}

static bk_err_t bk_avi_player_vfs_deinit(void)
{
    bk_err_t ret = BK_FAIL;

    return BK_OK;
}

static avdk_err_t display_frame_free_cb(void *frame)
{

    return AVDK_ERR_OK;
}

static bk_err_t bk_avi_player_reopen(void)
{
    bk_err_t ret = BK_OK;

    bk_avi_player_close();

    ret = bk_avi_player_open(&g_avi_player_config);
    if (ret != BK_OK) {
        LOGE("%s %d avi player open fail, ret: %d\r\n", __func__, __LINE__, ret);
        return ret;
    }

    handle = bk_avi_player_get_handle();

    return BK_OK;
}

static void dual_screen_avi_player_thread(beken_thread_arg_t data)
{
    bk_err_t ret;
    uint32_t delay_time = 0;
    uint32_t start_time, end_time;

    g_dual_screen_avi_player_is_running = true;
    rtos_set_semaphore(&g_dual_screen_avi_player_sem);

    handle->pos = 0;
    delay_time = 1000 / (uint32_t)handle->avi->fps;

    lcd_backlight_open(LCD_BL_IO);

    while (g_dual_screen_avi_player_is_running)
    {
        if (handle->pos == handle->video_num) {
            handle->pos = 0;
        }

        start_time = rtos_get_time();
        ret = bk_avi_player_video_parse();
        if (ret != BK_OK) {
            LOGE("%s %d bk_avi_video_prase_to_rgb565 failed\r\n", __func__, __LINE__);
            g_dual_screen_avi_fail_cnt++;
            if (g_dual_screen_avi_fail_cnt > FAIL_COUNT_MAX) {
                ret = bk_avi_player_reopen();
                if (ret != BK_OK) {
                    LOGE("%s %d avi player reopen fail, ret: %d\r\n", __func__, __LINE__, ret);
                    break;
                }
                g_dual_screen_avi_fail_cnt = 0;
                continue;
            }
            handle->pos++;
            continue;
        }
        handle->pos++;

        if (handle->segment_flag) {
            lcd_frame_buffer->frame = handle->segmentbuffer;
        } else {
            lcd_frame_buffer->frame = handle->framebuffer;
        }
        bk_display_flush(lcd_display_handle, lcd_frame_buffer, display_frame_free_cb);

        end_time = rtos_get_time();
        LOGV("bk_avi_player_video_parse time: %d ms\n", end_time - start_time);

        if (end_time - start_time > delay_time) {
            LOGI("bk_avi_player_video_parse time is too long, just delay 2ms\n");
            rtos_delay_milliseconds(2);
        } else {
            rtos_delay_milliseconds(delay_time);
        }
    }

    rtos_set_semaphore(&g_dual_screen_avi_player_sem);
    rtos_delete_thread(NULL);
}

bk_err_t bk_dual_screen_avi_player_start(char *file_path)
{
    bk_err_t ret = BK_OK;

    if (g_dual_screen_avi_player_is_running) {
        LOGW("%s already running\n", __func__);
        return BK_OK;
    }

    ret = bk_avi_player_vfs_init();
    if (ret != BK_OK) {
        LOGE("%s %d bk_avi_player_vfs_init failed\r\n", __func__, __LINE__);
        return ret;
    }

    g_avi_player_config.file_path = os_strdup(file_path);
    g_avi_player_config.output_format = AVI_PLAYER_OUTPUT_FORMAT_RGB565;
    g_avi_player_config.segment_flag = true;
    g_avi_player_config.rgb565_byte_swap_flag = true;
    ret = bk_avi_player_open(&g_avi_player_config);
    if (ret != BK_OK) {
        LOGE("%s %d bk_avi_player_open failed\r\n", __func__, __LINE__);
        bk_avi_player_vfs_deinit();
        return ret;
    }

    handle = bk_avi_player_get_handle();
    if (handle == NULL) {
        LOGE("%s %d bk_avi_player_get_handle failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_init_semaphore_ex(&g_dual_screen_avi_player_sem, 1, 0);
    if (ret != BK_OK) {
        LOGE("%s g_dual_screen_avi_player_sem init failed\n", __func__);
        bk_avi_player_close();
        bk_avi_player_vfs_deinit();
        return ret;
    }

    ret = bk_display_dual_spi_new(&lcd_display_handle, &dual_spi_ctlr_config);
    if (ret != BK_OK) {
        LOGE("%s %d bk_display_dual_spi_new failed\r\n", __func__, __LINE__);
        bk_avi_player_close();
        bk_avi_player_vfs_deinit();
        rtos_deinit_semaphore(&g_dual_screen_avi_player_sem);
        return ret;
    }

    ret = bk_display_open(lcd_display_handle);
    if (ret != BK_OK) {
        LOGE("%s %d bk_display_open failed\r\n", __func__, __LINE__);
        bk_avi_player_close();
        bk_avi_player_vfs_deinit();
        bk_display_delete(lcd_display_handle);
        rtos_deinit_semaphore(&g_dual_screen_avi_player_sem);
        return ret;
    }

    lcd_frame_buffer = os_malloc(sizeof(frame_buffer_t));
    if (lcd_frame_buffer == NULL) {
        LOGE("%s %d os_malloc failed\r\n", __func__, __LINE__);
        bk_avi_player_close();
        bk_avi_player_vfs_deinit();
        bk_display_close(lcd_display_handle);
        bk_display_delete(lcd_display_handle);
        rtos_deinit_semaphore(&g_dual_screen_avi_player_sem);
        return BK_FAIL;
    }

    lcd_frame_buffer->size = handle->frame_size;
    if (handle->segment_flag) {
        lcd_frame_buffer->width = handle->avi->height;
        lcd_frame_buffer->height = handle->avi->width;
    } else {
        lcd_frame_buffer->width = handle->avi->width;
        lcd_frame_buffer->height = handle->avi->height;
    }
    lcd_frame_buffer->fmt = PIXEL_FMT_RGB565;

    ret = rtos_create_thread(&g_dual_screen_avi_player_thread,
                             BEKEN_DEFAULT_WORKER_PRIORITY,
                             "dual_screen_avi_player_thread",
                             (beken_thread_function_t)dual_screen_avi_player_thread,
                             1024 * 4,
                             NULL);

    if (ret != BK_OK) {
        LOGE("%s, init thread failed\r\n", __func__);
        bk_avi_player_close();
        bk_avi_player_vfs_deinit();
        bk_display_close(lcd_display_handle);
        bk_display_delete(lcd_display_handle);
        rtos_deinit_semaphore(&g_dual_screen_avi_player_sem);
        os_free(lcd_frame_buffer);
        lcd_frame_buffer = NULL;
        return ret;
    }

    rtos_get_semaphore(&g_dual_screen_avi_player_sem, BEKEN_NEVER_TIMEOUT);

    LOGI("%s complete\n", __func__);

    return BK_OK;
}

bk_err_t bk_dual_screen_avi_player_stop(void)
{
    bk_err_t ret = BK_OK;

    if (!g_dual_screen_avi_player_is_running) {
        LOGW("%s already stopped\n", __func__);
        return BK_OK;
    }

    lcd_backlight_close(LCD_BL_IO);

    g_dual_screen_avi_player_is_running = false;

    ret = rtos_get_semaphore(&g_dual_screen_avi_player_sem, BEKEN_NEVER_TIMEOUT);
    if (ret != BK_OK) {
        LOGE("%s g_dual_screen_avi_player_sem get failed\n", __func__);
        return ret;
    }

    ret = bk_display_close(lcd_display_handle);
    if (ret != BK_OK) {
        LOGE("%s %d bk_display_close failed\r\n", __func__, __LINE__);
        return ret;
    }
    LOGD("bk_display_close success!\n");

    ret = bk_display_delete(lcd_display_handle);
    if (ret != BK_OK) {
        LOGE("%s %d bk_display_delete failed\r\n", __func__, __LINE__);
        return ret;
    }
    lcd_display_handle = NULL;
    LOGD("bk_display_delete success!\n");

    bk_avi_player_close();
    bk_avi_player_vfs_deinit();

    if (lcd_frame_buffer) {
        os_free(lcd_frame_buffer);
        lcd_frame_buffer = NULL;
    }

    rtos_deinit_semaphore(&g_dual_screen_avi_player_sem);
    if (ret != BK_OK) {
        LOGE("%s g_dual_screen_avi_player_sem deinit failed\n", __func__);
        return ret;
    }

    LOGI("%s complete\n", __func__);

    return BK_OK;
}
