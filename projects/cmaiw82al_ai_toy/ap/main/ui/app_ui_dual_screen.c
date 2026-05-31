#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <components/bk_display.h>
#include <components/log.h>
#include "bk_posix.h"
#include "avi_player.h"
#include "video_osi_wrapper.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_vendor.h"

#include "common.h"
#include "app_ui.h"
#include "display_module.h"

#define TAG "ui"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGV(format, ...) BK_LOGV(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

extern lv_vnd_config_t vendor_config;

#define UI_BG_COLOR       0x2C2C2E
#define UI_TEXT_COLOR     0xFFFFFF
#define UI_SAFE_FALLBACK_COLOR 0x000000
#define CMAIW82AL_EYE_AVI_WIDTH  320
#define CMAIW82AL_EYE_AVI_HEIGHT 160
#define EYE_AVI_FPS_LOG_INTERVAL_MS 5000
#define EYE_AVI_MIN_SLEEP_MS 2
#define EYE_AVI_PATH_MAX 64
#define EYE_AVI_CAROUSEL_DEFAULT_DWELL_SECONDS 6
#define EYE_AVI_CAROUSEL_MIN_DWELL_SECONDS 2
#define EYE_AVI_CAROUSEL_MAX_DWELL_SECONDS 60
#define EYE_AVI_BOOT_AUTOSTART_DEFAULT true
#define EYE_AVI_BOOT_START_DELAY_MS 0
#define EYE_AVI_SWITCH_MIN_INTERVAL_MS 350
#define EYE_AVI_OPEN_RETRY_MAX 3
#define EYE_AVI_OPEN_RETRY_DELAY_MS 300
#define EYE_AVI_AUTO_CAROUSEL_DEFAULT false
#define EYE_AVI_CAROUSEL_TASK_STACK_SIZE 4096
#define EYE_AVI_PREHEAT_TASK_STACK_SIZE 4096
#define EYE_AVI_PREHEAT_START_DELAY_MS 1200
#define EYE_AVI_PREHEAT_BETWEEN_MS 200
#define EYE_AVI_PREHEAT_ATTEMPTS 5
#define EYE_AVI_PREHEAT_RETRY_DELAY_MS 400
#define EYE_AVI_MAX_TRANSIENT_ERRORS 5
#define EYE_AVI_COLOR_PROBE_ENABLE 0
#define EYE_AVI_COLOR_SAMPLE_STEP_PIXELS 16
#define EYE_AVI_COLOR_ALERT_RATIO_PERMILLE 300
#define EYE_AVI_COLOR_LOG_INTERVAL_MS 5000
#define EYE_AVI_COLOR_CLOSE_THRESHOLD 80
#define EYE_AVI_FLUSH_LOG_INTERVAL_MS 5000
#define EYE_AVI_FRAME_FLAG 0x45594541u

typedef struct {
    lv_obj_t *root;
    lv_obj_t *container1;       // LCD1
    lv_obj_t *text_label1;
    lv_obj_t *container2;       // LCD2
    lv_obj_t *text_label2;
} app_ui_t;

typedef struct {
    app_ui_t ui;
    bool avi_enable;
    volatile bool avi_update_pending;
    volatile bool avi_handle_active;
    volatile bool avi_parse_active;
    bool avi_frame_visible;
    uint8_t avi_err_cnt;
    lv_img_dsc_t avi_desc;
    frame_buffer_t avi_frame;
    beken_semaphore_t avi_display_sem;
    bk_avi_player_t *avi_player;
    beken_thread_t avi_player_thread;
    uint32_t avi_frame_sequence;
    char current_eye_path[EYE_AVI_PATH_MAX];
    volatile bool eye_switch_pending;
    char requested_eye_name[EYE_AVI_PATH_MAX];
    uint32_t eye_switch_request_ms;
    char last_eye_request_name[EYE_AVI_PATH_MAX];
    uint32_t last_eye_request_ms;
    bool eye_carousel_enabled;
    uint32_t eye_carousel_dwell_ms;
    beken_thread_t eye_carousel_thread;
    beken_thread_t eye_preheat_thread;
} app_ui_ctx_t;

static app_ui_ctx_t s_ui_ctx = {0};

static const char *s_eye_avi_carousel_files[] = {
    EYE_AVI_IDLE_NAME,
    EYE_AVI_LISTEN_NAME,
    EYE_AVI_THINK_NAME,
    EYE_AVI_SPEAK_NAME,
    EYE_AVI_HAPPY_NAME,
    EYE_AVI_CAMERA_NAME,
    EYE_AVI_ERROR_NAME,
    EYE_AVI_SLEEP_NAME,
    EYE_AVI_AFRAID_NAME,
    EYE_AVI_ANGRY_NAME,
    EYE_AVI_BORED_NAME,
    EYE_AVI_CARING_NAME,
    EYE_AVI_DOUBTFUL_NAME,
    EYE_AVI_FROWNING_NAME,
    EYE_AVI_GRIMACING_NAME,
    EYE_AVI_SAD_NAME,
    EYE_AVI_SURPRISED_NAME,
    EYE_AVI_WINKING_NAME,
};

static const char *s_eye_avi_preheat_files[] = {
    EYE_AVI_LISTEN_NAME,
    EYE_AVI_HAPPY_NAME,
    EYE_AVI_SPEAK_NAME,
    EYE_AVI_DOUBTFUL_NAME,
};

static void _disp_avi_locked(void);
static void _refresh_static_fallback_locked(void);

static bk_avi_player_config_t s_avi_player_cfg = {
    .file_path = NULL,
    .output_format = AVI_PLAYER_OUTPUT_FORMAT_RGB565,
    .segment_flag = true,
    .rgb565_byte_swap_flag = true,
};

static bk_err_t _eye_avi_log_file_stat(const char *stage)
{
    struct stat st = {0};
    int ret = stat(s_ui_ctx.current_eye_path, &st);

    if (ret != 0) {
        LOGE("%s /sf0 stat fail path=%s ret=%d\r\n",
             stage,
             s_ui_ctx.current_eye_path,
             ret);
        return BK_FAIL;
    }

    LOGI("%s /sf0 stat path=%s size=%ld\r\n",
         stage,
         s_ui_ctx.current_eye_path,
         (long)st.st_size);
    return BK_OK;
}

static void _eye_avi_set_path(const char *file_name)
{
    if (file_name == NULL || file_name[0] == '\0') {
        file_name = EYE_AVI_DEFAULT_NAME;
    }

    snprintf(s_ui_ctx.current_eye_path,
             sizeof(s_ui_ctx.current_eye_path),
             "%s/%s",
             EYE_AVI_SPI_FLASH_ROOT,
             file_name);
    s_avi_player_cfg.file_path = s_ui_ctx.current_eye_path;
}

static bool _avi_player_resource_is_valid(bk_avi_player_t *handle)
{
    if (handle == NULL || handle->avi == NULL) {
        LOGE("avi resource invalid: no handle\r\n");
        return false;
    }

    if (handle->avi->width != CMAIW82AL_EYE_AVI_WIDTH ||
        handle->avi->height != CMAIW82AL_EYE_AVI_HEIGHT) {
        LOGE("avi resource invalid: %ldx%ld, expect %dx%d, path=%s\r\n",
             handle->avi->width,
             handle->avi->height,
             CMAIW82AL_EYE_AVI_WIDTH,
             CMAIW82AL_EYE_AVI_HEIGHT,
             s_ui_ctx.current_eye_path);
        return false;
    }

    return true;
}

static bk_err_t _avi_display_flush_complete(void *args)
{
    (void)args;

    if (s_ui_ctx.avi_display_sem != NULL) {
        rtos_set_semaphore(&s_ui_ctx.avi_display_sem);
    }

    return BK_OK;
}

static void _eye_avi_keep_last_frame_visible_locked(app_ui_ctx_t *ctx)
{
    if (ctx == NULL || ctx->ui.root == NULL) {
        return;
    }

    disp_disable_update();
    lv_obj_add_flag(ctx->ui.root, LV_OBJ_FLAG_HIDDEN);
    ctx->avi_enable = ctx->avi_frame_visible;
    LOGW("keep last avi frame visible\r\n");
}

static void _eye_avi_keep_last_frame_visible(app_ui_ctx_t *ctx)
{
    lv_vendor_disp_lock();
    _eye_avi_keep_last_frame_visible_locked(ctx);
    lv_vendor_disp_unlock();
}

static void _eye_avi_show_fallback_if_no_valid_frame(app_ui_ctx_t *ctx, const char *reason)
{
    if (ctx == NULL) {
        return;
    }

    if (!ctx->avi_frame_visible) {
        LOGW("%s; no valid avi frame, show static fallback\r\n", reason ? reason : "avi unavailable");
        app_ui_refresh_static_fallback();
        return;
    }

    _eye_avi_keep_last_frame_visible(ctx);
}

static void _avi_player_close_current(void)
{
    if (s_ui_ctx.avi_player != NULL) {
        bk_avi_player_close();
        s_ui_ctx.avi_player = NULL;
    }
}

static bk_err_t _avi_player_fill_desc(void)
{
    s_ui_ctx.avi_desc.header.cf = LV_IMG_CF_TRUE_COLOR;
    s_ui_ctx.avi_desc.header.w = s_ui_ctx.avi_player->avi->height;
    s_ui_ctx.avi_desc.header.h = s_ui_ctx.avi_player->avi->width;
    s_ui_ctx.avi_desc.data_size = s_ui_ctx.avi_desc.header.w * s_ui_ctx.avi_desc.header.h * 2;

    os_memset(&s_ui_ctx.avi_frame, 0, sizeof(s_ui_ctx.avi_frame));
    s_ui_ctx.avi_frame.size = s_ui_ctx.avi_player->frame_size;
    s_ui_ctx.avi_frame.length = s_ui_ctx.avi_player->frame_size;
    s_ui_ctx.avi_frame.width = s_ui_ctx.avi_desc.header.w;
    s_ui_ctx.avi_frame.height = s_ui_ctx.avi_desc.header.h;
    s_ui_ctx.avi_frame.fmt = PIXEL_FMT_RGB565;
    return BK_OK;
}

static uint32_t _eye_avi_sample_hash_update(uint32_t hash, uint8_t value)
{
    hash ^= value;
    return hash * 16777619u;
}

static void _eye_avi_rgb565_to_rgb888(uint16_t pixel, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t r5 = (pixel >> 11) & 0x1f;
    uint32_t g6 = (pixel >> 5) & 0x3f;
    uint32_t b5 = pixel & 0x1f;

    *r = (uint8_t)((r5 * 255u) / 31u);
    *g = (uint8_t)((g6 * 255u) / 63u);
    *b = (uint8_t)((b5 * 255u) / 31u);
}

static bool _eye_avi_rgb565_near_color(uint16_t pixel,
                                       uint8_t target_r,
                                       uint8_t target_g,
                                       uint8_t target_b,
                                       uint32_t threshold)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    int32_t dr = 0;
    int32_t dg = 0;
    int32_t db = 0;

    _eye_avi_rgb565_to_rgb888(pixel, &r, &g, &b);
    dr = (int32_t)r - target_r;
    dg = (int32_t)g - target_g;
    db = (int32_t)b - target_b;

    return (uint32_t)(dr * dr + dg * dg + db * db) <= threshold * threshold;
}

static bool _eye_avi_rgb565_pair_near_color(uint16_t be_pixel,
                                            uint16_t le_pixel,
                                            uint8_t target_r,
                                            uint8_t target_g,
                                            uint8_t target_b)
{
    return _eye_avi_rgb565_near_color(be_pixel,
                                      target_r,
                                      target_g,
                                      target_b,
                                      EYE_AVI_COLOR_CLOSE_THRESHOLD) ||
           _eye_avi_rgb565_near_color(le_pixel,
                                      target_r,
                                      target_g,
                                      target_b,
                                      EYE_AVI_COLOR_CLOSE_THRESHOLD);
}

static uint32_t _eye_avi_probe_frame_colors(app_ui_ctx_t *ctx,
                                            bk_avi_player_t *handle,
                                            const uint8_t *src,
                                            uint32_t size)
{
    static uint32_t s_last_probe_log_time;
    uint32_t pixel_count = size / 2;
    uint32_t sample_step = EYE_AVI_COLOR_SAMPLE_STEP_PIXELS;
    uint32_t samples = 0;
    uint32_t fallback_blue = 0;
    uint32_t fallback_yellow = 0;
    uint32_t cyan = 0;
    uint32_t bright = 0;
    uint32_t sample_hash = 2166136261u;
    uint32_t fallback_blue_pm = 0;
    uint32_t fallback_yellow_pm = 0;
    uint32_t cyan_pm = 0;
    uint32_t bright_pm = 0;
    uint32_t now = 0;
    bool alert = false;

    if (ctx == NULL || handle == NULL || src == NULL || size < 2) {
        return 0;
    }

    if (!EYE_AVI_COLOR_PROBE_ENABLE) {
        return sample_hash;
    }

    if (sample_step == 0) {
        sample_step = 1;
    }

    for (uint32_t pixel = 0; pixel < pixel_count; pixel += sample_step) {
        uint32_t offset = pixel * 2;
        uint16_t be_pixel = ((uint16_t)src[offset] << 8) | src[offset + 1];
        uint16_t le_pixel = ((uint16_t)src[offset + 1] << 8) | src[offset];
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        sample_hash = _eye_avi_sample_hash_update(sample_hash, src[offset]);
        sample_hash = _eye_avi_sample_hash_update(sample_hash, src[offset + 1]);
        samples++;

        if (_eye_avi_rgb565_pair_near_color(be_pixel, le_pixel, 0, 91, 255)) {
            fallback_blue++;
        }
        if (_eye_avi_rgb565_pair_near_color(be_pixel, le_pixel, 255, 212, 0)) {
            fallback_yellow++;
        }
        if (_eye_avi_rgb565_pair_near_color(be_pixel, le_pixel, 102, 205, 220)) {
            cyan++;
        }

        _eye_avi_rgb565_to_rgb888(be_pixel, &r, &g, &b);
        if (r > 180 && g > 180 && b > 180) {
            bright++;
        }
    }

    if (samples == 0) {
        return 0;
    }

    fallback_blue_pm = (fallback_blue * 1000u) / samples;
    fallback_yellow_pm = (fallback_yellow * 1000u) / samples;
    cyan_pm = (cyan * 1000u) / samples;
    bright_pm = (bright * 1000u) / samples;
    alert = fallback_blue_pm >= EYE_AVI_COLOR_ALERT_RATIO_PERMILLE ||
            fallback_yellow_pm >= EYE_AVI_COLOR_ALERT_RATIO_PERMILLE ||
            cyan_pm >= EYE_AVI_COLOR_ALERT_RATIO_PERMILLE;

    now = rtos_get_time();
    if (!alert && now - s_last_probe_log_time < EYE_AVI_COLOR_LOG_INTERVAL_MS) {
        return sample_hash;
    }
    s_last_probe_log_time = now;

    if (alert) {
        LOGW("EYE_FRAME_COLOR_ALERT path=%s pos=%u samples=%u fallback_blue_pm=%u fallback_yellow_pm=%u cyan_pm=%u bright_pm=%u sample_hash=0x%08x buf=0x%08x size=%u\r\n",
             s_ui_ctx.current_eye_path,
             handle->pos,
             samples,
             fallback_blue_pm,
             fallback_yellow_pm,
             cyan_pm,
             bright_pm,
             sample_hash,
             (uint32_t)src,
             size);
    }

    return sample_hash;
}

static bk_err_t _avi_player_publish_frame_locked(app_ui_ctx_t *ctx, bk_avi_player_t *handle)
{
    static uint32_t s_last_flush_log_time;
    const uint8_t *src = NULL;
    uint32_t sample_hash = 0;
    uint32_t now = 0;
    avdk_err_t flush_ret = BK_FAIL;
    bk_err_t wait_ret = BK_OK;

    if (ctx == NULL || handle == NULL || vendor_config.handle == NULL || ctx->avi_frame.size == 0) {
        return BK_ERR_PARAM;
    }

    if (handle->segment_flag == true) {
        src = (const uint8_t *)handle->segmentbuffer;
    } else {
        src = (const uint8_t *)handle->framebuffer;
    }

    if (src == NULL) {
        return BK_FAIL;
    }

    while (ctx->avi_display_sem != NULL &&
           rtos_get_semaphore(&ctx->avi_display_sem, BEKEN_NO_WAIT) == BK_OK) {
    }

    ctx->avi_frame.frame = (uint8_t *)src;
    ctx->avi_frame.flag = EYE_AVI_FRAME_FLAG;
    ctx->avi_frame.size = handle->frame_size;
    ctx->avi_frame.length = handle->frame_size;
    ctx->avi_frame.fmt = PIXEL_FMT_RGB565;
    ctx->avi_frame.sequence = ++ctx->avi_frame_sequence;
    ctx->avi_frame.timestamp = rtos_get_time();
    if (handle->segment_flag == true) {
        ctx->avi_frame.width = handle->avi->height;
        ctx->avi_frame.height = handle->avi->width;
    } else {
        ctx->avi_frame.width = handle->avi->width;
        ctx->avi_frame.height = handle->avi->height;
    }

    sample_hash = _eye_avi_probe_frame_colors(ctx, handle, src, ctx->avi_frame.size);
    now = rtos_get_time();
    if (ctx->avi_frame.sequence <= 5 ||
        now - s_last_flush_log_time >= EYE_AVI_FLUSH_LOG_INTERVAL_MS) {
        s_last_flush_log_time = now;
        LOGI("EYE_AVI_FLUSH seq=%u path=%s pos=%u source=%s frame=0x%08x buf=0x%08x size=%u wxh=%ux%u flag=0x%08x sample_hash=0x%08x\r\n",
             ctx->avi_frame.sequence,
             s_ui_ctx.current_eye_path,
             handle->pos,
             handle->segment_flag ? "segment" : "frame",
             (uint32_t)&ctx->avi_frame,
             (uint32_t)ctx->avi_frame.frame,
             ctx->avi_frame.size,
             ctx->avi_frame.width,
             ctx->avi_frame.height,
             ctx->avi_frame.flag,
             sample_hash);
    }

    flush_ret = bk_display_flush(vendor_config.handle, &ctx->avi_frame, _avi_display_flush_complete);
    if (flush_ret != BK_OK) {
        LOGE("avi display flush failed ret=%d frame=0x%08x buf=0x%08x size=%u\r\n",
             flush_ret,
             (uint32_t)&ctx->avi_frame,
             (uint32_t)ctx->avi_frame.frame,
             ctx->avi_frame.size);
        return flush_ret;
    }

    if (ctx->avi_display_sem != NULL) {
        wait_ret = rtos_get_semaphore(&ctx->avi_display_sem, 1000);
        if (wait_ret != BK_OK) {
            LOGE("avi display flush wait failed ret=%d frame=0x%08x buf=0x%08x size=%u\r\n",
                 wait_ret,
                 (uint32_t)&ctx->avi_frame,
                 (uint32_t)ctx->avi_frame.frame,
                 ctx->avi_frame.size);
            return wait_ret;
        }
    }

    ctx->avi_frame_visible = true;

    return BK_OK;
}

static bk_err_t _avi_player_publish_frame(app_ui_ctx_t *ctx, bk_avi_player_t *handle)
{
    return _avi_player_publish_frame_locked(ctx, handle);
}

static bk_err_t _avi_player_decode_next_frame(app_ui_ctx_t *ctx, bk_avi_player_t *handle)
{
    bk_err_t ret = BK_OK;

    if (ctx == NULL || handle == NULL || handle->avi == NULL) {
        return BK_ERR_PARAM;
    }

    if (handle->pos == handle->video_num) {
        handle->pos = 0;
    }

    ctx->avi_parse_active = true;
    ret = bk_avi_player_video_parse();
    ctx->avi_parse_active = false;
    return ret;
}

static int _avi_player_reopen(void)
{
    bk_err_t ret = BK_OK;

    _avi_player_close_current();

    if (_eye_avi_log_file_stat("AVI_OPEN") != BK_OK) {
        return BK_FAIL;
    }

    if (s_avi_player_cfg.file_path == NULL) {
        _eye_avi_set_path(EYE_AVI_DEFAULT_NAME);
    }

    ret = bk_avi_player_open(&s_avi_player_cfg);
    if (ret != BK_OK) {
        LOGE("avi player open fail, ret: %d\r\n", ret);
        return ret;
    }

    s_ui_ctx.avi_player = bk_avi_player_get_handle();
    if (s_ui_ctx.avi_player == NULL) {
        LOGE("get avi player handle fail\r\n");
        return BK_FAIL;
    }

    if (!_avi_player_resource_is_valid(s_ui_ctx.avi_player)) {
        _avi_player_close_current();
        return BK_ERR_PARAM;
    }

    ret = _avi_player_fill_desc();
    if (ret != BK_OK) {
        _avi_player_close_current();
        return ret;
    }

    return BK_OK;
}

static bk_err_t _avi_player_reopen_with_retry(void)
{
    bk_err_t ret = BK_FAIL;

    for (uint32_t attempt = 0; attempt < EYE_AVI_OPEN_RETRY_MAX; attempt++) {
        ret = _avi_player_reopen();
        if (ret == BK_OK) {
            return BK_OK;
        }

        LOGW("EYE_AVI open retry path=%s attempt=%u/%u ret=%d\r\n",
             s_ui_ctx.current_eye_path,
             attempt + 1,
             EYE_AVI_OPEN_RETRY_MAX,
             ret);
        rtos_delay_milliseconds(EYE_AVI_OPEN_RETRY_DELAY_MS);
    }

    return ret;
}

static bk_err_t _eye_avi_apply_pending_switch(app_ui_ctx_t *ctx)
{
    bk_err_t ret = BK_OK;
    char requested_eye_name[EYE_AVI_PATH_MAX] = {0};
    uint32_t request_ms = 0;
    uint32_t apply_ms = 0;
    uint32_t open_start_ms = 0;
    uint32_t open_end_ms = 0;
    uint32_t decode_start_ms = 0;
    uint32_t decode_end_ms = 0;
    uint32_t publish_start_ms = 0;
    uint32_t publish_end_ms = 0;
    uint32_t display_start_ms = 0;
    uint32_t display_end_ms = 0;

    if (ctx == NULL || !ctx->eye_switch_pending) {
        return BK_OK;
    }

    apply_ms = rtos_get_time();
    request_ms = ctx->eye_switch_request_ms;
    if (request_ms == 0) {
        request_ms = apply_ms;
    }

    snprintf(requested_eye_name, sizeof(requested_eye_name), "%s", ctx->requested_eye_name);
    if (requested_eye_name[0] == '\0') {
        ctx->eye_switch_pending = false;
        ctx->avi_update_pending = false;
        ctx->avi_enable = false;
        return BK_ERR_PARAM;
    }

    ctx->eye_switch_pending = false;
    ctx->requested_eye_name[0] = '\0';
    ctx->avi_update_pending = true;
    ctx->avi_enable = false;
    ctx->avi_handle_active = true;
    ctx->avi_err_cnt = 0;

    LOGI("EYE_SWITCH apply file=%s\r\n", requested_eye_name);
    _eye_avi_set_path(requested_eye_name);
    open_start_ms = rtos_get_time();
    ret = _avi_player_reopen_with_retry();
    open_end_ms = rtos_get_time();
    if (ret != BK_OK) {
        LOGE("eye avi switch open failed file=%s ret=%d\r\n", requested_eye_name, ret);
        LOGI("EYE_SWITCH_LATENCY file=%s queued=%u open=%u decode=%u publish=%u display=%u total=%u ret=%d\r\n",
             requested_eye_name,
             apply_ms - request_ms,
             open_end_ms - open_start_ms,
             0,
             0,
             0,
             open_end_ms - request_ms,
             ret);
        ctx->avi_handle_active = false;
        ctx->avi_update_pending = false;
        LOGW("avi reopen failed; keep last frame if available\r\n");
        ctx->avi_enable = ctx->avi_frame_visible;
        _eye_avi_show_fallback_if_no_valid_frame(ctx, "avi reopen failed");
        return ret;
    }

    decode_start_ms = rtos_get_time();
    ret = _avi_player_decode_next_frame(ctx, ctx->avi_player);
    decode_end_ms = rtos_get_time();
    if (ret != BK_OK) {
        LOGE("eye avi switch decode failed file=%s ret=%d\r\n", requested_eye_name, ret);
        LOGI("EYE_SWITCH_LATENCY file=%s queued=%u open=%u decode=%u publish=%u display=%u total=%u ret=%d\r\n",
             requested_eye_name,
             apply_ms - request_ms,
             open_end_ms - open_start_ms,
             decode_end_ms - decode_start_ms,
             0,
             0,
             decode_end_ms - request_ms,
             ret);
        ctx->avi_handle_active = false;
        ctx->avi_update_pending = false;
        ctx->avi_enable = ctx->avi_frame_visible;
        _eye_avi_show_fallback_if_no_valid_frame(ctx, "avi decode failed");
        return ret;
    }
    ctx->avi_player->pos++;

    publish_start_ms = rtos_get_time();
    ret = _avi_player_publish_frame_locked(ctx, ctx->avi_player);
    publish_end_ms = rtos_get_time();
    if (ret == BK_OK) {
        display_start_ms = rtos_get_time();
        lv_vendor_disp_lock();
        _disp_avi_locked();
        lv_vendor_disp_unlock();
        display_end_ms = rtos_get_time();
    }

    if (ret != BK_OK) {
        LOGE("eye avi switch publish failed file=%s ret=%d\r\n", requested_eye_name, ret);
        LOGI("EYE_SWITCH_LATENCY file=%s queued=%u open=%u decode=%u publish=%u display=%u total=%u ret=%d\r\n",
             requested_eye_name,
             apply_ms - request_ms,
             open_end_ms - open_start_ms,
             decode_end_ms - decode_start_ms,
             publish_end_ms - publish_start_ms,
             0,
             publish_end_ms - request_ms,
             ret);
        ctx->avi_handle_active = false;
        ctx->avi_update_pending = false;
        ctx->avi_enable = ctx->avi_frame_visible;
        _eye_avi_show_fallback_if_no_valid_frame(ctx, "avi publish failed");
        return ret;
    }

    LOGI("EYE_SWITCH_LATENCY file=%s queued=%u open=%u decode=%u publish=%u display=%u total=%u ret=%d\r\n",
         requested_eye_name,
         apply_ms - request_ms,
         open_end_ms - open_start_ms,
         decode_end_ms - decode_start_ms,
         publish_end_ms - publish_start_ms,
         display_end_ms - display_start_ms,
         display_end_ms - request_ms,
         ret);

    ctx->avi_handle_active = false;
    ctx->avi_update_pending = false;
    ctx->avi_enable = true;
    return BK_OK;
}

static void avi_player_task_main(beken_thread_arg_t arg)
{
    bk_err_t ret = BK_OK;
    uint32_t frame_period = 40;
    uint32_t target_fps = 25;
    uint32_t start_time, end_time, now_time;
    uint32_t fps_log_start_time;
    uint32_t fps_frame_count;
    uint32_t fps_decode_error_count;
    uint32_t fps_decode_overrun_count;
    uint32_t next_frame_time;
    app_ui_ctx_t *ctx = (app_ui_ctx_t *)arg;
    bk_avi_player_t *handle = NULL;

    fps_log_start_time = rtos_get_time();
    fps_frame_count = 0;
    fps_decode_error_count = 0;
    fps_decode_overrun_count = 0;
    next_frame_time = fps_log_start_time;
    ctx->avi_enable = true;

    while (1) {
        if (ctx->eye_switch_pending) {
            ret = _eye_avi_apply_pending_switch(ctx);
            fps_log_start_time = rtos_get_time();
            fps_frame_count = 0;
            fps_decode_error_count = 0;
            fps_decode_overrun_count = 0;
            next_frame_time = fps_log_start_time;
            if (ret != BK_OK) {
                rtos_delay_milliseconds(frame_period);
            }
            continue;
        }

        if (!ctx->avi_enable || ctx->avi_update_pending) {
            rtos_delay_milliseconds(frame_period);
            continue;
        }

        ctx->avi_handle_active = true;
        if (!ctx->avi_enable || ctx->avi_update_pending) {
            ctx->avi_handle_active = false;
            rtos_delay_milliseconds(frame_period);
            continue;
        }

        handle = ctx->avi_player;

        if (handle == NULL || handle->avi == NULL) {
            ctx->avi_handle_active = false;
            rtos_delay_milliseconds(frame_period);
            continue;
        }

        if (handle->avi->fps > 0) {
            target_fps = (uint32_t)handle->avi->fps;
            frame_period = 1000 / target_fps;
            if (frame_period < EYE_AVI_MIN_SLEEP_MS) {
                frame_period = EYE_AVI_MIN_SLEEP_MS;
            }
        }

        if (ctx->avi_err_cnt >= EYE_AVI_MAX_TRANSIENT_ERRORS) {
            LOGW("avi transient errors exceed %u; keep last frame and continue\r\n",
                 EYE_AVI_MAX_TRANSIENT_ERRORS);
            ctx->avi_err_cnt = 0;
            ctx->avi_handle_active = false;
            _eye_avi_show_fallback_if_no_valid_frame(ctx, "avi transient decode failures");
            if (handle->video_num > 0) {
                handle->pos = (handle->pos + 1) % handle->video_num;
            }
            rtos_delay_milliseconds(frame_period);
            continue;
        }

        start_time = rtos_get_time();
        next_frame_time = start_time + frame_period;
        ret = _avi_player_decode_next_frame(ctx, handle);
        if (ret != BK_OK) {
            LOGE("bk_avi_video_prase_to_rgb565 fail, ret: %d\r\n", ret);
            handle->pos++;
            ctx->avi_err_cnt++;
            fps_decode_error_count++;
            ctx->avi_handle_active = false;
            now_time = rtos_get_time();
            if (now_time < next_frame_time) {
                rtos_delay_milliseconds(next_frame_time - now_time);
            } else {
                fps_decode_overrun_count++;
                rtos_delay_milliseconds(EYE_AVI_MIN_SLEEP_MS);
            }
            continue;
        } else {
            ctx->avi_err_cnt = 0;
        }
        end_time = rtos_get_time();
        LOGV("bk_avi_player_video_parse time: %d ms\n", end_time - start_time);

        handle->pos++;
        fps_frame_count++;

        ret = _avi_player_publish_frame(ctx, handle);
        if (ret != BK_OK) {
            LOGE("publish avi frame fail, ret: %d\r\n", ret);
            ctx->avi_err_cnt++;
            fps_decode_error_count++;
        }
        ctx->avi_handle_active = false;

        now_time = rtos_get_time();
        if (now_time - fps_log_start_time >= EYE_AVI_FPS_LOG_INTERVAL_MS) {
            uint32_t elapsed = now_time - fps_log_start_time;
            uint32_t avg_x100 = elapsed ? (fps_frame_count * 100000) / elapsed : 0;
            LOGI("EYE_AVI_FPS frames=%u errors=%u overruns=%u avg=%u.%02u target=%u period=%u\r\n",
                 fps_frame_count,
                 fps_decode_error_count,
                 fps_decode_overrun_count,
                 avg_x100 / 100,
                 avg_x100 % 100,
                 target_fps,
                 frame_period);
            fps_log_start_time = now_time;
            fps_frame_count = 0;
            fps_decode_error_count = 0;
            fps_decode_overrun_count = 0;
        }

        if (now_time < next_frame_time) {
            rtos_delay_milliseconds(next_frame_time - now_time);
        } else {
            fps_decode_overrun_count++;
            LOGV("bk_avi_player_video_parse time is too long, time: %d ms\n", end_time - start_time);
            rtos_delay_milliseconds(EYE_AVI_MIN_SLEEP_MS);
        }
    }

    ctx->avi_parse_active = false;
    ctx->avi_handle_active = false;
    ctx->avi_player_thread = NULL;
    rtos_delete_thread(NULL);
}

static int _avi_player_start_thread(void)
{
    bk_err_t ret = BK_OK;

    if (s_ui_ctx.avi_player_thread == NULL) {
        ret = rtos_create_thread(&s_ui_ctx.avi_player_thread,
                                 6,
                                 "avi_player_thread",
                                 (beken_thread_function_t)avi_player_task_main,
                                 1024 * 4,
                                 (void *)&s_ui_ctx);
        if (ret != BK_OK) {
            LOGE("create avi player thread fail, ret: %d\r\n", ret);
            return BK_FAIL;
        }
    } else {
        return BK_OK;
    }

    return BK_OK;
}

static int _avi_init(void)
{
    bk_err_t ret = BK_OK;

    s_ui_ctx.avi_update_pending = true;
    s_ui_ctx.eye_switch_pending = false;
    s_ui_ctx.requested_eye_name[0] = '\0';
    s_ui_ctx.avi_enable = false;
    s_ui_ctx.avi_err_cnt = 0;

    if (s_ui_ctx.avi_display_sem == NULL) {
        ret = rtos_init_semaphore_ex(&s_ui_ctx.avi_display_sem, 1, 0);
        if (ret != BK_OK) {
            LOGE("avi display sem init failed, ret=%d\r\n", ret);
            s_ui_ctx.avi_update_pending = false;
            return ret;
        }
    }

    ret = _avi_player_reopen();
    if (ret != BK_OK) {
        LOGE("open avi player fail, ret: %d\r\n", ret);
        s_ui_ctx.avi_update_pending = false;
        return ret;
    }

    ret = _avi_player_decode_next_frame(&s_ui_ctx, s_ui_ctx.avi_player);
    if (ret != BK_OK) {
        LOGE("decode first avi frame fail, ret: %d\r\n", ret);
        _avi_player_close_current();
        s_ui_ctx.avi_update_pending = false;
        return ret;
    }
    s_ui_ctx.avi_player->pos++;

    ret = _avi_player_publish_frame_locked(&s_ui_ctx, s_ui_ctx.avi_player);
    if (ret != BK_OK) {
        LOGE("publish first avi frame fail, ret: %d\r\n", ret);
        _avi_player_close_current();
        s_ui_ctx.avi_update_pending = false;
        return ret;
    }

    s_ui_ctx.avi_update_pending = false;
    _disp_avi_locked();

    ret = _avi_player_start_thread();
    if (ret != BK_OK) {
        s_ui_ctx.avi_enable = false;
    }

    return ret;
}

static int _find_active_code(char *text, char *code, int code_len)
{
    int i = 0;
    bool found = false;
    char *p = text;

    if (text == NULL || code == NULL || code_len <= 0) {
        return BK_ERR_PARAM;
    }

    code[0] = '\0';

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (i >= code_len - 1) {
                break;
            }
            code[i++] = *p;
            found = true;
        } else {
            if (found) {
                break;
            }
        }
        p++;
    }

    code[i] = '\0';

    return BK_OK;
}

static void _disp_active_code(char *text)
{
    static bool act_code_flag = false;
    char code[8] = {0};
    char code1[4] = {0};
    char code2[4] = {0};

    LOGI("ui disp active code\r\n");

    if (act_code_flag) {
        return;
    }

    _find_active_code(text, code, sizeof(code));
    if (os_strlen(code) != 6) {
        LOGE("active code len error\r\n");
        return;
    }

    s_ui_ctx.avi_enable = false;
    s_ui_ctx.avi_frame_visible = false;

    disp_enable_update();
    os_strncpy(code1, code, 3);
    os_strncpy(code2, code + 3, 3);
    lv_label_set_text(s_ui_ctx.ui.text_label1, code1);
    lv_label_set_text(s_ui_ctx.ui.text_label2, code2);
    lv_obj_clear_flag(s_ui_ctx.ui.root, LV_OBJ_FLAG_HIDDEN);

    act_code_flag = true;

    return;
}

static void _disp_upgrade(void)
{
    LOGI("ui disp upgrade\r\n");

    s_ui_ctx.avi_enable = false;
    s_ui_ctx.avi_frame_visible = false;

    disp_enable_update();
    lv_label_set_text(s_ui_ctx.ui.text_label1, "Upgrade");
    lv_label_set_text(s_ui_ctx.ui.text_label2, "......");
    lv_obj_clear_flag(s_ui_ctx.ui.root, LV_OBJ_FLAG_HIDDEN);
}

static void _refresh_static_fallback_locked(void)
{
    if (s_ui_ctx.ui.root == NULL) {
        return;
    }

    s_ui_ctx.avi_enable = false;
    s_ui_ctx.avi_frame_visible = false;
    disp_enable_update();
    lv_obj_set_style_bg_color(s_ui_ctx.ui.root, lv_color_hex(UI_SAFE_FALLBACK_COLOR), 0);
    lv_obj_set_style_bg_color(s_ui_ctx.ui.container1, lv_color_hex(UI_SAFE_FALLBACK_COLOR), 0);
    lv_obj_set_style_bg_color(s_ui_ctx.ui.container2, lv_color_hex(UI_SAFE_FALLBACK_COLOR), 0);
    lv_label_set_text(s_ui_ctx.ui.text_label1, "");
    lv_label_set_text(s_ui_ctx.ui.text_label2, "");
    lv_obj_clear_flag(s_ui_ctx.ui.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_ui_ctx.ui.root);

    if (lv_scr_act() != NULL) {
        lv_obj_invalidate(lv_scr_act());
    }

    lv_refr_now(NULL);
    LOGI("static fallback refresh: safe blank\r\n");
}

static void _disp_avi_locked(void)
{
    LOGI("ui disp avi\r\n");

    if (!s_ui_ctx.avi_frame_visible) {
        if (s_ui_ctx.eye_switch_pending || s_ui_ctx.avi_update_pending) {
            LOGI("AVI pending; keep current display until first frame\r\n");
            return;
        }

        LOGW("AVI unavailable; show static fallback\r\n");
        _refresh_static_fallback_locked();
        return;
    }

    if (s_ui_ctx.avi_player == NULL) {
        LOGW("AVI handle unavailable; keep last frame visible\r\n");
        _eye_avi_keep_last_frame_visible_locked(&s_ui_ctx);
        return;
    }

    disp_disable_update();
    lv_obj_add_flag(s_ui_ctx.ui.root, LV_OBJ_FLAG_HIDDEN);
    s_ui_ctx.avi_enable = true;
}

static void _disp_avi(void)
{
    lv_vendor_disp_lock();
    _disp_avi_locked();
    lv_vendor_disp_unlock();
}

void app_ui_refresh_static_fallback(void)
{
    lv_vendor_disp_lock();
    _refresh_static_fallback_locked();
    lv_vendor_disp_unlock();
}

bk_err_t app_ui_prepare_eye_resource_update(void)
{
    bool was_enabled = s_ui_ctx.avi_enable;

    s_ui_ctx.avi_update_pending = true;
    s_ui_ctx.avi_enable = false;
    s_ui_ctx.eye_switch_pending = false;
    s_ui_ctx.requested_eye_name[0] = '\0';
    s_ui_ctx.avi_err_cnt = 0;

    if (s_ui_ctx.ui.root != NULL) {
        lv_vendor_disp_lock();
        _disp_upgrade();
        lv_obj_invalidate(s_ui_ctx.ui.root);
        lv_refr_now(NULL);
        lv_vendor_disp_unlock();
    }

    for (uint32_t i = 0; i < 50 && (s_ui_ctx.avi_parse_active || s_ui_ctx.avi_handle_active); i++) {
        rtos_delay_milliseconds(10);
    }

    if (s_ui_ctx.avi_parse_active || s_ui_ctx.avi_handle_active) {
        LOGW("avi player still active before eye resource update\r\n");
        s_ui_ctx.avi_update_pending = false;
        s_ui_ctx.avi_enable = was_enabled;
        return BK_FAIL;
    }

    if (s_ui_ctx.avi_player != NULL) {
        bk_avi_player_close();
        s_ui_ctx.avi_player = NULL;
    }

    LOGI("eye resource update prepared\r\n");
    return BK_OK;
}

bk_err_t app_ui_reopen_eye_resource(void)
{
    bk_err_t ret = BK_OK;

    _eye_avi_set_path(EYE_AVI_DEFAULT_NAME);
    ret = _avi_player_reopen();
    if (ret != BK_OK) {
        LOGE("reopen eye resource fail, ret=%d\r\n", ret);
        s_ui_ctx.avi_update_pending = false;
        app_ui_refresh_static_fallback();
        return ret;
    }

    ret = _avi_player_decode_next_frame(&s_ui_ctx, s_ui_ctx.avi_player);
    if (ret != BK_OK) {
        LOGE("decode reopened eye frame fail, ret=%d\r\n", ret);
        s_ui_ctx.avi_update_pending = false;
        app_ui_refresh_static_fallback();
        return ret;
    }
    s_ui_ctx.avi_player->pos++;

    ret = _avi_player_publish_frame_locked(&s_ui_ctx, s_ui_ctx.avi_player);
    if (ret != BK_OK) {
        LOGE("publish reopened eye frame fail, ret=%d\r\n", ret);
        s_ui_ctx.avi_update_pending = false;
        app_ui_refresh_static_fallback();
        return ret;
    }

    ret = _avi_player_start_thread();
    if (ret != BK_OK) {
        LOGE("restart eye resource thread failed, ret=%d\r\n", ret);
        s_ui_ctx.avi_update_pending = false;
        app_ui_refresh_static_fallback();
        return ret;
    }

    LOGI("eye resource reopened\r\n");
    s_ui_ctx.avi_update_pending = false;
    s_ui_ctx.avi_enable = true;
    lv_vendor_disp_lock();
    _disp_avi_locked();
    lv_vendor_disp_unlock();
    return BK_OK;
}

int app_ui_init(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);

    s_ui_ctx.ui.root = lv_obj_create(screen);
    lv_obj_set_size(s_ui_ctx.ui.root, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(s_ui_ctx.ui.root, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_border_width(s_ui_ctx.ui.root, 0, 0);
    lv_obj_set_style_pad_all(s_ui_ctx.ui.root, 0, 0);
    // 默认隐藏
    lv_obj_add_flag(s_ui_ctx.ui.root, LV_OBJ_FLAG_HIDDEN);
    
    // 创建content1对象 - LCD1
    s_ui_ctx.ui.container1 = lv_obj_create(s_ui_ctx.ui.root);
    lv_obj_set_size(s_ui_ctx.ui.container1, LCD_WIDTH, LCD_HEIGHT / 2);
    lv_obj_set_pos(s_ui_ctx.ui.container1, 0, 0);
    lv_obj_set_style_bg_color(s_ui_ctx.ui.container1, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_border_width(s_ui_ctx.ui.container1, 0, 0);
    lv_obj_set_style_pad_all(s_ui_ctx.ui.container1, 0, 0);
    
    // 创建content2对象 - LCD2
    s_ui_ctx.ui.container2 = lv_obj_create(s_ui_ctx.ui.root);
    lv_obj_set_size(s_ui_ctx.ui.container2, LCD_WIDTH, LCD_HEIGHT / 2);
    lv_obj_set_pos(s_ui_ctx.ui.container2, 0, LCD_HEIGHT / 2);
    lv_obj_set_style_bg_color(s_ui_ctx.ui.container2, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_border_width(s_ui_ctx.ui.container2, 0, 0);
    lv_obj_set_style_pad_all(s_ui_ctx.ui.container2, 0, 0);

    s_ui_ctx.ui.text_label1 = lv_label_create(s_ui_ctx.ui.container1);
    lv_obj_set_style_text_color(s_ui_ctx.ui.text_label1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ui_ctx.ui.text_label1, &lv_font_montserrat_32, 0);
    lv_obj_align(s_ui_ctx.ui.text_label1, LV_ALIGN_CENTER, 0, 0);
    
    s_ui_ctx.ui.text_label2 = lv_label_create(s_ui_ctx.ui.container2);
    lv_obj_set_style_text_color(s_ui_ctx.ui.text_label2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ui_ctx.ui.text_label2, &lv_font_montserrat_32, 0);
    lv_obj_align(s_ui_ctx.ui.text_label2, LV_ALIGN_CENTER, 0, 0);
    
    lv_scr_load(screen);
    _eye_avi_set_path(EYE_AVI_DEFAULT_NAME);

    LOGI("EYE_AVI boot autostart pending; fallback reserved for errors\r\n");
    _refresh_static_fallback_locked();
    LOGI("safe blank before backlight\r\n");

    if (EYE_AVI_AUTO_CAROUSEL_DEFAULT) {
        (void)app_ui_eye_carousel_start(EYE_AVI_CAROUSEL_DEFAULT_DWELL_SECONDS);
    }

    return BK_OK;
}

void app_ui_display_wifi_status(bool status)
{
    return;
}

void app_ui_display_chat_message(char *text)
{
    if (text == NULL) {
        return;
    }

    if (os_strncmp(text, DISP_ACTIVED_TEXT, os_strlen(DISP_ACTIVED_TEXT)) == 0) {
        _disp_avi();
        return;
    } else if (os_strncmp(text, DISP_ACTIVE_CODE_TEXT, os_strlen(DISP_ACTIVE_CODE_TEXT)) == 0) {
        _disp_active_code(text);
        return;
    } else if (os_strncmp(text, DISP_UPGRADE_TEXT, os_strlen(DISP_UPGRADE_TEXT)) == 0) {
        _disp_upgrade();
        return;
    }

    return;
}

static const char *_eye_avi_name_for_emotion(const char *emo)
{
    if (emo == NULL || emo[0] == '\0') {
        return EYE_AVI_IDLE_NAME;
    }

    if (os_strcasecmp(emo, "neutral") == 0 ||
        os_strcasecmp(emo, "idle") == 0 ||
        os_strcasecmp(emo, "default") == 0) {
        return EYE_AVI_IDLE_NAME;
    }

    if (os_strcasecmp(emo, "happy") == 0 ||
        os_strcasecmp(emo, "joy") == 0) {
        return EYE_AVI_HAPPY_NAME;
    }

    if (os_strcasecmp(emo, "smile") == 0 ||
        os_strcasecmp(emo, "smiling") == 0 ||
        os_strcasecmp(emo, "speak") == 0 ||
        os_strcasecmp(emo, "speaking") == 0) {
        return EYE_AVI_SPEAK_NAME;
    }

    if (os_strcasecmp(emo, "listen") == 0 ||
        os_strcasecmp(emo, "listening") == 0 ||
        os_strcasecmp(emo, "curious") == 0) {
        return EYE_AVI_LISTEN_NAME;
    }

    if (os_strcasecmp(emo, "think") == 0 ||
        os_strcasecmp(emo, "thinking") == 0 ||
        os_strcasecmp(emo, "pensive") == 0) {
        return EYE_AVI_THINK_NAME;
    }

    if (os_strcasecmp(emo, "camera") == 0 ||
        os_strcasecmp(emo, "photo") == 0) {
        return EYE_AVI_CAMERA_NAME;
    }

    if (os_strcasecmp(emo, "error") == 0 ||
        os_strcasecmp(emo, "confused") == 0) {
        return EYE_AVI_ERROR_NAME;
    }

    if (os_strcasecmp(emo, "sleep") == 0 ||
        os_strcasecmp(emo, "tired") == 0) {
        return EYE_AVI_SLEEP_NAME;
    }

    if (os_strcasecmp(emo, "afraid") == 0) {
        return EYE_AVI_AFRAID_NAME;
    }

    if (os_strcasecmp(emo, "angry") == 0) {
        return EYE_AVI_ANGRY_NAME;
    }

    if (os_strcasecmp(emo, "bored") == 0) {
        return EYE_AVI_BORED_NAME;
    }

    if (os_strcasecmp(emo, "caring") == 0) {
        return EYE_AVI_CARING_NAME;
    }

    if (os_strcasecmp(emo, "doubtful") == 0) {
        return EYE_AVI_DOUBTFUL_NAME;
    }

    if (os_strcasecmp(emo, "frowning") == 0) {
        return EYE_AVI_FROWNING_NAME;
    }

    if (os_strcasecmp(emo, "grimacing") == 0) {
        return EYE_AVI_GRIMACING_NAME;
    }

    if (os_strcasecmp(emo, "sad") == 0) {
        return EYE_AVI_SAD_NAME;
    }

    if (os_strcasecmp(emo, "surprised") == 0 ||
        os_strcasecmp(emo, "suprised") == 0) {
        return EYE_AVI_SURPRISED_NAME;
    }

    if (os_strcasecmp(emo, "winking") == 0 ||
        os_strcasecmp(emo, "wink") == 0) {
        return EYE_AVI_WINKING_NAME;
    }

    return EYE_AVI_IDLE_NAME;
}

static bk_err_t _eye_avi_switch_to(const char *file_name)
{
    char requested_eye_path[EYE_AVI_PATH_MAX] = {0};
    uint32_t now_ms = 0;

    if (file_name == NULL || file_name[0] == '\0') {
        return BK_ERR_PARAM;
    }

    now_ms = rtos_get_time();
    snprintf(requested_eye_path,
             sizeof(requested_eye_path),
             "%s/%s",
             EYE_AVI_SPI_FLASH_ROOT,
             file_name);

    if (s_ui_ctx.eye_switch_pending &&
        os_strcmp(file_name, s_ui_ctx.requested_eye_name) == 0) {
        LOGI("EYE_SWITCH skip duplicate pending file=%s\r\n", file_name);
        return BK_OK;
    }

    if (s_ui_ctx.last_eye_request_name[0] != '\0' &&
        os_strcmp(file_name, s_ui_ctx.last_eye_request_name) == 0 &&
        now_ms - s_ui_ctx.last_eye_request_ms < EYE_AVI_SWITCH_MIN_INTERVAL_MS) {
        LOGI("EYE_SWITCH throttle file=%s elapsed=%u min=%u\r\n",
             file_name,
             now_ms - s_ui_ctx.last_eye_request_ms,
             EYE_AVI_SWITCH_MIN_INTERVAL_MS);
        return BK_OK;
    }

    if (!s_ui_ctx.eye_switch_pending &&
        os_strcmp(requested_eye_path, s_ui_ctx.current_eye_path) == 0 &&
        s_ui_ctx.avi_player != NULL) {
        s_ui_ctx.avi_enable = true;
        LOGI("EYE_SWITCH skip current file=%s\r\n", file_name);
        return BK_OK;
    }

    snprintf(s_ui_ctx.requested_eye_name, sizeof(s_ui_ctx.requested_eye_name), "%s", file_name);
    snprintf(s_ui_ctx.last_eye_request_name, sizeof(s_ui_ctx.last_eye_request_name), "%s", file_name);
    s_ui_ctx.last_eye_request_ms = now_ms;
    s_ui_ctx.eye_switch_pending = true;
    s_ui_ctx.eye_switch_request_ms = now_ms;
    s_ui_ctx.avi_update_pending = true;
    s_ui_ctx.avi_enable = false;
    s_ui_ctx.avi_err_cnt = 0;

    LOGI("EYE_SWITCH request file=%s\r\n", file_name);
    if (_avi_player_start_thread() != BK_OK) {
        s_ui_ctx.eye_switch_pending = false;
        s_ui_ctx.avi_update_pending = false;
        app_ui_refresh_static_fallback();
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_ui_eye_debug_play(const char *file_name)
{
    bk_err_t ret = BK_OK;

    if (file_name == NULL || file_name[0] == '\0') {
        return BK_ERR_PARAM;
    }

    LOGI("EYE_DEBUG play file=%s\r\n", file_name);
    ret = _eye_avi_switch_to(file_name);
    if (ret != BK_OK) {
        LOGW("EYE_DEBUG play failed file=%s ret=%d\r\n", file_name, ret);
    }

    return ret;
}

bk_err_t app_ui_start_default_eye(void)
{
    if (!EYE_AVI_BOOT_AUTOSTART_DEFAULT) {
        LOGW("EYE_AVI boot autostart disabled; static fallback will remain visible\r\n");
        return BK_OK;
    }

    LOGI("EYE_AVI boot autostart file=%s\r\n", EYE_AVI_BOOT_NAME);
    rtos_delay_milliseconds(EYE_AVI_BOOT_START_DELAY_MS);
    return _eye_avi_switch_to(EYE_AVI_BOOT_NAME);
}

static void eye_carousel_task_main(beken_thread_arg_t arg)
{
    uint32_t index = 0;
    app_ui_ctx_t *ctx = (app_ui_ctx_t *)arg;
    const uint32_t file_count = sizeof(s_eye_avi_carousel_files) / sizeof(s_eye_avi_carousel_files[0]);

    while (ctx->eye_carousel_enabled) {
        const char *file_name = s_eye_avi_carousel_files[index % file_count];

        LOGI("EYE_CAROUSEL file=%s dwell_ms=%u index=%u/%u\r\n",
             file_name,
             ctx->eye_carousel_dwell_ms,
             (index % file_count) + 1,
             file_count);

        (void)app_ui_eye_debug_play(file_name);

        for (uint32_t elapsed = 0;
             ctx->eye_carousel_enabled && elapsed < ctx->eye_carousel_dwell_ms;
             elapsed += 100) {
            rtos_delay_milliseconds(100);
        }

        index++;
    }

    ctx->eye_carousel_thread = NULL;
    rtos_delete_thread(NULL);
}

static void eye_preheat_task_main(beken_thread_arg_t arg)
{
    app_ui_ctx_t *ctx = (app_ui_ctx_t *)arg;
    const uint32_t file_count = sizeof(s_eye_avi_preheat_files) / sizeof(s_eye_avi_preheat_files[0]);
    char path[EYE_AVI_PATH_MAX] = {0};

    rtos_delay_milliseconds(EYE_AVI_PREHEAT_START_DELAY_MS);

    for (uint32_t i = 0; i < file_count; i++) {
        const char *file_name = s_eye_avi_preheat_files[i];
        bk_err_t ret = BK_OK;

        if (file_name == NULL || file_name[0] == '\0') {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", EYE_AVI_SPI_FLASH_ROOT, file_name);
        for (uint32_t attempt = 1; attempt <= EYE_AVI_PREHEAT_ATTEMPTS; attempt++) {
            ret = bk_video_osi_preload_avi_cache(path);
            LOGI("EYE_PREHEAT file=%s attempt=%u ret=%d\r\n", file_name, attempt, ret);
            if (ret == BK_OK) {
                break;
            }
            rtos_delay_milliseconds(EYE_AVI_PREHEAT_RETRY_DELAY_MS);
        }
        rtos_delay_milliseconds(EYE_AVI_PREHEAT_BETWEEN_MS);
    }

    ctx->eye_preheat_thread = NULL;
    rtos_delete_thread(NULL);
}

bk_err_t app_ui_start_eye_preheat(void)
{
    bk_err_t ret = BK_OK;

    if (s_ui_ctx.eye_preheat_thread != NULL) {
        return BK_OK;
    }

    ret = rtos_create_thread(&s_ui_ctx.eye_preheat_thread,
                             4,
                             "eye_preheat",
                             (beken_thread_function_t)eye_preheat_task_main,
                             EYE_AVI_PREHEAT_TASK_STACK_SIZE,
                             (void *)&s_ui_ctx);
    if (ret != BK_OK) {
        s_ui_ctx.eye_preheat_thread = NULL;
        LOGW("EYE_PREHEAT create failed ret=%d\r\n", ret);
    }

    return ret;
}

bk_err_t app_ui_eye_carousel_start(uint32_t dwell_seconds)
{
    bk_err_t ret = BK_OK;

    if (dwell_seconds < EYE_AVI_CAROUSEL_MIN_DWELL_SECONDS) {
        dwell_seconds = EYE_AVI_CAROUSEL_MIN_DWELL_SECONDS;
    } else if (dwell_seconds > EYE_AVI_CAROUSEL_MAX_DWELL_SECONDS) {
        dwell_seconds = EYE_AVI_CAROUSEL_MAX_DWELL_SECONDS;
    }

    s_ui_ctx.eye_carousel_dwell_ms = dwell_seconds * 1000;
    s_ui_ctx.eye_carousel_enabled = true;

    if (s_ui_ctx.eye_carousel_thread != NULL) {
        LOGI("EYE_CAROUSEL already running dwell_ms=%u\r\n", s_ui_ctx.eye_carousel_dwell_ms);
        return BK_OK;
    }

    ret = rtos_create_thread(&s_ui_ctx.eye_carousel_thread,
                             5,
                             "eye_carousel",
                             (beken_thread_function_t)eye_carousel_task_main,
                             EYE_AVI_CAROUSEL_TASK_STACK_SIZE,
                             (void *)&s_ui_ctx);
    if (ret != BK_OK) {
        s_ui_ctx.eye_carousel_enabled = false;
        LOGE("EYE_CAROUSEL create thread failed ret=%d\r\n", ret);
        return ret;
    }

    LOGI("EYE_CAROUSEL start dwell_ms=%u\r\n", s_ui_ctx.eye_carousel_dwell_ms);
    return BK_OK;
}

void app_ui_eye_carousel_stop(void)
{
    s_ui_ctx.eye_carousel_enabled = false;
    LOGI("EYE_CAROUSEL stop\r\n");
}

void app_ui_display_chat_emotion(char *emo)
{
    const char *file_name = _eye_avi_name_for_emotion(emo);
    bk_err_t ret = BK_OK;

    if (s_ui_ctx.eye_carousel_enabled) {
        LOGI("eye emotion ignored while carousel active emo=%s\r\n", emo ? emo : "(null)");
        return;
    }

    LOGI("eye emotion switch emo=%s file=%s\r\n", emo ? emo : "(null)", file_name);
    ret = _eye_avi_switch_to(file_name);
    if (ret != BK_OK) {
        LOGW("eye emotion switch failed emo=%s file=%s ret=%d\r\n",
             emo ? emo : "(null)",
             file_name,
             ret);
    }
}
