#include "os/os.h"
#include "os/mem.h"
#include "driver/dma2d.h"
#include "components/bk_display.h"
#include "lcd_panel_devices.h"
#include "driver/gpio.h"
#include "gpio_driver.h"
#include "frame_buffer.h"
#include "components/bk_jpeg_decode/bk_jpeg_decode_hw.h"
#include "bk_posix.h"
#include "driver/flash_partition.h"

#define TAG "ota_disp"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)
#define LOGV(...) BK_LOGV(TAG, ##__VA_ARGS__)

#define LCD_BL_GPIO GPIO_25

static beken_semaphore_t ota_disp_dma2d_sem = NULL;
static frame_buffer_t *g_jpeg_frame = NULL;
static frame_buffer_t *g_dec_out_frame = NULL;
static uint8_t *g_rgb565_frame = NULL;
static frame_buffer_t *g_disp_frame = NULL;
static bk_display_ctlr_handle_t lcd_display_handle = NULL;
static bk_jpeg_decode_hw_handle_t ota_disp_jpeg_decode_handle = NULL;

extern bk_display_dual_spi_ctlr_config_t dual_spi_ctlr_config;

static void ota_disp_dma2d_config_error(void *arg)
{
    LOGD("%s \n", __func__);
}

static void ota_disp_dma2d_transfer_error(void *arg)
{
    LOGE("%s \n", __func__);
}

static void ota_disp_dma2d_transfer_complete(void *arg)
{
    rtos_set_semaphore(&ota_disp_dma2d_sem);
}

static bk_err_t ota_disp_dma2d_yuyv2rgb565_init(void)
{
    bk_err_t ret;

    ret = rtos_init_semaphore_ex(&ota_disp_dma2d_sem, 1, 0);
    if (BK_OK != ret) {
        LOGE("%s %d avi_player_dma2d_sem init failed\n", __func__, __LINE__);
        return ret;
    }

    bk_dma2d_driver_init();
    bk_dma2d_register_int_callback_isr(DMA2D_CFG_ERROR_ISR, ota_disp_dma2d_config_error, NULL);
    bk_dma2d_register_int_callback_isr(DMA2D_TRANS_ERROR_ISR, ota_disp_dma2d_transfer_error, NULL);
    bk_dma2d_register_int_callback_isr(DMA2D_TRANS_COMPLETE_ISR, ota_disp_dma2d_transfer_complete, NULL);
    bk_dma2d_int_enable(DMA2D_CFG_ERROR | DMA2D_TRANS_ERROR | DMA2D_TRANS_COMPLETE, 1);

    return ret;
}

static bk_err_t ota_disp_dma2d_yuyv2rgb565_deinit(void)
{
    bk_err_t ret;

    bk_dma2d_stop_transfer();
    bk_dma2d_int_enable(DMA2D_CFG_ERROR | DMA2D_TRANS_ERROR | DMA2D_TRANS_COMPLETE, 0);
    bk_dma2d_driver_deinit();
    ret = rtos_deinit_semaphore(&ota_disp_dma2d_sem);
    if (BK_OK != ret) {
        LOGE("%s %d avi_player_dma2d_sem deinit failed\n", __func__, __LINE__);
    }

    return ret;
}

static void ota_disp_dma2d_yuyv2rgb565(void *src, const void *dst, uint16_t width, uint16_t height, bool byte_swap)
{
    dma2d_memcpy_pfc_t dma2d_memcpy_pfc = {0};

    dma2d_memcpy_pfc.input_addr = (char *)src;
    dma2d_memcpy_pfc.output_addr = (char *)dst;
    dma2d_memcpy_pfc.mode = DMA2D_M2M_PFC;
    dma2d_memcpy_pfc.input_color_mode = DMA2D_INPUT_YUYV;
    dma2d_memcpy_pfc.output_color_mode = DMA2D_OUTPUT_RGB565;
    dma2d_memcpy_pfc.src_pixel_byte = TWO_BYTES;
    dma2d_memcpy_pfc.dst_pixel_byte = TWO_BYTES;
    dma2d_memcpy_pfc.dma2d_width = width;
    dma2d_memcpy_pfc.dma2d_height = height;
    dma2d_memcpy_pfc.src_frame_width = width;
    dma2d_memcpy_pfc.src_frame_height = height;
    dma2d_memcpy_pfc.dst_frame_width = width;
    dma2d_memcpy_pfc.dst_frame_height = height;
    dma2d_memcpy_pfc.src_frame_xpos = 0;
    dma2d_memcpy_pfc.src_frame_ypos = 0;
    dma2d_memcpy_pfc.dst_frame_xpos = 0;
    dma2d_memcpy_pfc.dst_frame_ypos = 0;
    dma2d_memcpy_pfc.input_red_blue_swap = 0;
    dma2d_memcpy_pfc.output_red_blue_swap = 0;

    if (byte_swap) {
        dma2d_memcpy_pfc.out_byte_by_byte_reverse = 1;
    } else {
        dma2d_memcpy_pfc.out_byte_by_byte_reverse = 0;
    }

    bk_dma2d_memcpy_or_pixel_convert(&dma2d_memcpy_pfc);
    bk_dma2d_start_transfer();

    rtos_get_semaphore(&ota_disp_dma2d_sem, BEKEN_NEVER_TIMEOUT);
}

static bk_err_t ota_disp_jpeg_decode_complete(uint32_t format_type, uint32_t result, frame_buffer_t *out_frame)
{
    if (result == BK_OK) {
        LOGV("%s, %d, jpeg decode success! format_type: %d, out_frame: %p\n", __func__, __LINE__, format_type, out_frame);
    } else {
        LOGE("%s, %d, jpeg decode failed! format_type: %d, result: %d, out_frame: %p\n", __func__, __LINE__, format_type, result, out_frame);
    }

    return BK_OK;
}

static bk_err_t ota_disp_jpeg_decode_in_complete(frame_buffer_t *in_frame)
{
    LOGV("%s %d in_frame: %p\n", __func__, __LINE__, in_frame);
    return BK_OK;
}

static bk_jpeg_decode_hw_config_t ota_disp_jpeg_decode_config = {
    .decode_cbs = {
        .in_complete = ota_disp_jpeg_decode_in_complete,
        .out_complete = ota_disp_jpeg_decode_complete,}
};

static avdk_err_t ota_display_frame_free_cb(void *frame)
{

    return AVDK_ERR_OK;
}

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
    /* app_vfs_init() owns /sf0 mounting on CMAiW82AL. */
    return BK_OK;
}

static bk_err_t bk_ota_display_vfs_init(void)
{
    bk_err_t ret = BK_FAIL;

    do {
        ret = _fs_mount();
        if (BK_OK != ret) {
            LOGE("[%s][%d] mount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
            break;
        }

        LOGI("[%s][%d] mount success\r\n", __FUNCTION__, __LINE__);
    } while(0);

    return ret;
}

static bk_err_t bk_ota_display_vfs_deinit(void)
{
    return BK_OK;
}

static bk_err_t bk_image_filelen_get(char *filename)
{
    bk_err_t ret = BK_FAIL;
    struct stat statbuf;

    do {
        if (!filename) {
            LOGE("[%s][%d] filename param is null.\r\n", __FUNCTION__, __LINE__);
            ret = BK_ERR_PARAM;
            break;
        }

        ret = stat(filename, &statbuf);
        if (BK_OK != ret) {
            LOGE("[%s][%d] sta fail:%s\r\n", __FUNCTION__, __LINE__, filename);
            break;
        }

        ret = statbuf.st_size;
        LOGI("[%s][%d] %s size:%d\r\n", __FUNCTION__, __LINE__, filename, ret);
    } while(0);

    return ret;
}

static bk_err_t bk_image_file_read(char *filename, void *read_buff, uint32_t data_len)
{
    int ret = BK_FAIL;
    int fd = -1;
    int read_len = 0;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        LOGE("[%s][%d] %s open fail\r\n", __FUNCTION__, __LINE__, filename);
        return BK_FAIL;
    }

    read_len = read(fd, read_buff, data_len);
    if (read_len < 0) {
        LOGE("[%s][%d] read file fail.\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    ret = close(fd);
    if (ret < 0) {
        LOGE("[%s][%d] close file fail.\r\n", __FUNCTION__, __LINE__);
        return BK_FAIL;
    }

    return BK_OK;
}

void bk_ota_display_init(void)
{
    bk_err_t ret = BK_OK;

    g_jpeg_frame = os_malloc(sizeof(frame_buffer_t));
    if (g_jpeg_frame == NULL) {
        LOGE("[%s][%d] g_jpeg_frame malloc failed\r\n", __FUNCTION__, __LINE__);
        return;
    }
    os_memset(g_jpeg_frame, 0, sizeof(frame_buffer_t));

    g_dec_out_frame = os_malloc(sizeof(frame_buffer_t));
    if (g_dec_out_frame == NULL) {
        LOGE("[%s][%d] g_dec_out_frame malloc failed\r\n", __FUNCTION__, __LINE__);
        os_free(g_jpeg_frame);
        g_jpeg_frame = NULL;
        return;
    }
    os_memset(g_dec_out_frame, 0, sizeof(frame_buffer_t));

    g_dec_out_frame->size = dual_spi_ctlr_config.lcd0_config.lcd_device->width * dual_spi_ctlr_config.lcd0_config.lcd_device->height * 2 * 2;
    g_dec_out_frame->frame = psram_malloc(g_dec_out_frame->size);
    if (g_dec_out_frame->frame == NULL) {
        LOGE("[%s][%d] g_dec_out_frame->frame malloc failed\r\n", __FUNCTION__, __LINE__);
        goto fail;
    }

    g_rgb565_frame = psram_malloc(g_dec_out_frame->size);
    if (g_rgb565_frame == NULL) {
        LOGE("[%s][%d] g_rgb565_frame malloc failed\r\n", __FUNCTION__, __LINE__);
        goto fail;
    }

    g_disp_frame = os_malloc(sizeof(frame_buffer_t));
    if (g_disp_frame == NULL) {
        LOGE("[%s][%d] g_disp_frame malloc failed\r\n", __FUNCTION__, __LINE__);
        goto fail;
    }
    os_memset(g_disp_frame, 0, sizeof(frame_buffer_t));

    g_disp_frame->frame = psram_malloc(g_dec_out_frame->size);
    if (g_disp_frame->frame == NULL) {
        LOGE("[%s][%d] g_disp_frame->frame malloc failed\r\n", __FUNCTION__, __LINE__);
        goto fail;
    }

    g_disp_frame->size = g_dec_out_frame->size;
    g_disp_frame->width = dual_spi_ctlr_config.lcd0_config.lcd_device->width;
    g_disp_frame->height = dual_spi_ctlr_config.lcd0_config.lcd_device->height * 2;
    g_disp_frame->fmt = PIXEL_FMT_RGB565;

    ret = bk_display_dual_spi_new(&lcd_display_handle, &dual_spi_ctlr_config);
    if (ret != BK_OK) {
        LOGE("[%s][%d] bk_display_dual_spi_new failed\r\n", __FUNCTION__, __LINE__);
        goto fail;
    }

    bk_hardware_jpeg_decode_new(&ota_disp_jpeg_decode_handle, &ota_disp_jpeg_decode_config);
    bk_jpeg_decode_hw_open(ota_disp_jpeg_decode_handle);

    ota_disp_dma2d_yuyv2rgb565_init();

    return;

fail:
    if (g_jpeg_frame) {
        os_free(g_jpeg_frame);
        g_jpeg_frame = NULL;
    }

    if (g_dec_out_frame && g_dec_out_frame->frame) {
        psram_free(g_dec_out_frame->frame);
        g_dec_out_frame->frame = NULL;
    }

    if (g_dec_out_frame) {
        os_free(g_dec_out_frame);
        g_dec_out_frame = NULL;
    }

    if (g_rgb565_frame) {
        psram_free(g_rgb565_frame);
        g_rgb565_frame = NULL;
    }

    if (g_disp_frame && g_disp_frame->frame) {
        psram_free(g_disp_frame->frame);
        g_disp_frame->frame = NULL;
    }

    if (g_disp_frame) {
        os_free(g_disp_frame);
        g_disp_frame = NULL;
    }
}

void bk_ota_display_deinit(void)
{
    bk_display_delete(lcd_display_handle);

    bk_jpeg_decode_hw_close(ota_disp_jpeg_decode_handle);
    bk_jpeg_decode_hw_delete(ota_disp_jpeg_decode_handle);

    ota_disp_dma2d_yuyv2rgb565_deinit();

    if (g_jpeg_frame) {
        os_free(g_jpeg_frame);
        g_jpeg_frame = NULL;
    }

    if (g_dec_out_frame && g_dec_out_frame->frame) {
        psram_free(g_dec_out_frame->frame);
        g_dec_out_frame->frame = NULL;
    }

    if (g_dec_out_frame) {
        os_free(g_dec_out_frame);
        g_dec_out_frame = NULL;
    }

    if (g_rgb565_frame) {
        psram_free(g_rgb565_frame);
        g_rgb565_frame = NULL;
    }

    if (g_disp_frame && g_disp_frame->frame) {
        psram_free(g_disp_frame->frame);
        g_disp_frame->frame = NULL;
    }

    if (g_disp_frame) {
        os_free(g_disp_frame);
        g_disp_frame = NULL;
    }
}

bk_err_t bk_ota_image_display_open(char *filename)
{
    bk_err_t ret = BK_OK;

    ret = bk_ota_display_vfs_init();
    if (ret != BK_OK) {
        LOGE("[%s][%d] bk_ota_display_vfs_init failed\r\n", __FUNCTION__, __LINE__);
        return ret;
    }

    g_jpeg_frame->length = bk_image_filelen_get(filename);
    if (g_jpeg_frame->length <= 0) {
        LOGE("[%s][%d] bk_image_filelen_get failed\r\n", __FUNCTION__, __LINE__);
        bk_ota_display_vfs_deinit();
        return BK_FAIL;
    }

    g_jpeg_frame->frame = psram_malloc(g_jpeg_frame->length);
    if (g_jpeg_frame->frame == NULL) {
        LOGE("[%s][%d] g_jpeg_frame->frame malloc failed\r\n", __FUNCTION__, __LINE__);
        bk_ota_display_vfs_deinit();
        return BK_FAIL;
    }

    ret = bk_image_file_read(filename, g_jpeg_frame->frame, g_jpeg_frame->length);
    if (ret != BK_OK) {
        LOGE("[%s][%d] bk_image_file_read failed\r\n", __FUNCTION__, __LINE__);
        psram_free(g_jpeg_frame->frame);
        g_jpeg_frame->frame = NULL;
        bk_ota_display_vfs_deinit();
        return BK_FAIL;
    }

    LOGI("[%s][%d] jpeg frame read success\r\n", __FUNCTION__, __LINE__);

    ret = bk_jpeg_decode_hw_decode(ota_disp_jpeg_decode_handle, g_jpeg_frame, g_dec_out_frame);
    if (ret != BK_OK) {
        LOGE("%s bk_jpeg_decode_hw_decode fail %d\n", __func__, ret);
        psram_free(g_jpeg_frame->frame);
        g_jpeg_frame->frame = NULL;
        return ret;
    }

    if (g_jpeg_frame && g_jpeg_frame->frame) {
        psram_free(g_jpeg_frame->frame);
        g_jpeg_frame->frame = NULL;
    }

    if (g_jpeg_frame) {
        os_free(g_jpeg_frame);
        g_jpeg_frame = NULL;
    }

    LOGI("[%s][%d] jpeg decode success, width: %d, height: %d\r\n", __FUNCTION__, __LINE__, g_dec_out_frame->width, g_dec_out_frame->height);

    ota_disp_dma2d_yuyv2rgb565(g_dec_out_frame->frame, g_rgb565_frame, g_dec_out_frame->width, g_dec_out_frame->height, true);

    for (int i = 0; i < g_dec_out_frame->height; i++) {
        os_memcpy(g_disp_frame->frame + i * g_dec_out_frame->width, g_rgb565_frame + i * g_dec_out_frame->width * 2, g_dec_out_frame->width);
        os_memcpy(g_disp_frame->frame + (g_dec_out_frame->width >> 1) * g_dec_out_frame->height * 2 + i * g_dec_out_frame->width, g_rgb565_frame + i * g_dec_out_frame->width * 2 + g_dec_out_frame->width, g_dec_out_frame->width);
    }

    bk_display_open(lcd_display_handle);
    lcd_backlight_open(LCD_BL_GPIO);

    bk_display_flush(lcd_display_handle, g_disp_frame, ota_display_frame_free_cb);

    return BK_OK;
}

bk_err_t bk_ota_image_display_close(void)
{
    lcd_backlight_close(LCD_BL_GPIO);

    bk_display_close(lcd_display_handle);

    bk_ota_display_vfs_deinit();

    return BK_OK;
}
