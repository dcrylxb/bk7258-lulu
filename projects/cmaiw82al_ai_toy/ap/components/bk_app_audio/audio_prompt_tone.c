#include <os/mem.h>
#include <components/log.h>
#include <components/bk_player_service.h>
#include <components/bk_audio/audio_pipeline/rb_port.h>
#include <components/bk_audio/audio_streams/onboard_speaker_stream.h>

#include "bk_app_audio.h"
#include "audio_prompt_tone.h"

#define TAG "audio"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    uint8_t                 port_id;
    uint32_t                rb_size;
    audio_dec_type_t        dec_type;
    bool                    playback_finished;
    bk_player_handle_t      player_handle;
    audio_port_handle_t     player_out_port;
    audio_element_handle_t  spk_stream;
} audio_prompt_tone_ctx_t;

static void audio_prompt_tone_detach_speaker_port(audio_prompt_tone_ctx_t *ctx);

static int _player_event_handler(int event, void *data, void *args)
{
    audio_prompt_tone_ctx_t *ctx = (audio_prompt_tone_ctx_t *)args;
    
    LOGD("event: %d\r\n", event);

    if (ctx == NULL || ctx->spk_stream == NULL) {
        LOGE("invalid param\r\n");
        return BK_FAIL;
    }

    if (event == PLAYER_EVENT_MUSIC_INFO) {
#if CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE
        audio_element_info_t *music_info = (audio_element_info_t *)data;
        uint8_t dig_gain = 0;
        uint8_t ana_gain = 0;

        onboard_speaker_stream_get_digital_gain(ctx->spk_stream, &dig_gain);
        onboard_speaker_stream_get_analog_gain(ctx->spk_stream, &ana_gain);

        audio_port_info_t port_info = DEFAULT_AUDIO_PORT_INFO();
        port_info.chl_num = music_info->channels;
        port_info.sample_rate = music_info->sample_rates;
        port_info.dig_gain = dig_gain;
        port_info.ana_gain = ana_gain;
        port_info.bits = music_info->bits;
        port_info.port_id = ctx->port_id;
        port_info.priority = AUDIO_SPK_PORT_PRIORITY_TONE;
        port_info.port = ctx->player_out_port;
        if (BK_OK != onboard_speaker_stream_set_input_port_info(ctx->spk_stream, &port_info)) {
            LOGE("onboard_speaker_stream_set_input_port_info fail\r\n");
            return BK_FAIL;
        }
        LOGD("sample_rates: %d, bits: %d, channels: %d\n", music_info->sample_rates, music_info->bits, music_info->channels);
        LOGD("port_id: %d, priority: %d, port: %p\n", port_info.port_id, port_info.priority, port_info.port);
#else
        LOGE("CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE is not set\r\n");
        return BK_FAIL;
#endif
    } else if (event == PLAYER_EVENT_START) {
        ctx->playback_finished = false;
        LOGD("play tone start\r\n");
    } else if (event == PLAYER_EVENT_FINISH) {
        ctx->playback_finished = true;
        LOGD("play tone finish\r\n");
    }

    return BK_OK;
}

static bk_err_t audio_prompt_tone_create_player(audio_prompt_tone_ctx_t *ctx)
{
    if (ctx == NULL) {
        LOGE("ctx is NULL");
        return BK_FAIL;
    }

    if (ctx->player_handle != NULL) {
        return BK_OK;
    }

    bk_player_cfg_t player_cfg = DEFAULT_PLAYER_NOT_PLAYBACK_CONFIG();
    player_cfg.spk_type = SPK_TYPE_INVALID;
    player_cfg.event_handle = _player_event_handler;
    player_cfg.args = ctx;

    ctx->player_handle = bk_player_create(&player_cfg);
    if (ctx->player_handle == NULL) {
        LOGE("bk_player_init fail\r\n");
        return BK_FAIL;
    }

    return BK_OK;
}

static bk_err_t audio_prompt_tone_create_output_port(audio_prompt_tone_ctx_t *ctx)
{
    if (ctx == NULL) {
        LOGE("ctx is NULL");
        return BK_FAIL;
    }

    if (ctx->player_out_port != NULL) {
        return BK_OK;
    }

    ringbuf_port_cfg_t rb_cfg = RINGBUF_PORT_CFG_DEFAULT();
    rb_cfg.ringbuf_size = ctx->rb_size;

    ctx->player_out_port = ringbuf_port_init(&rb_cfg);
    if (ctx->player_out_port == NULL) {
        LOGE("ringbuf_port_init fail\r\n");
        return BK_FAIL;
    }

    return BK_OK;
}

static bk_err_t audio_prompt_tone_destroy_player(audio_prompt_tone_ctx_t *ctx, bool stop_first)
{
    bk_err_t ret = BK_OK;

    if (ctx == NULL || ctx->player_handle == NULL) {
        return BK_OK;
    }

    if (stop_first) {
        ret = bk_player_stop(ctx->player_handle);
        if (ret != BK_OK) {
            LOGW("bk_player_stop failed, ret:%d\r\n", ret);
        }
    }

    audio_prompt_tone_detach_speaker_port(ctx);

    ret = bk_player_destroy(ctx->player_handle);
    if (ret != BK_OK) {
        LOGW("bk_player_destroy failed, ret:%d\r\n", ret);
    }
    ctx->player_handle = NULL;
    ctx->playback_finished = false;

    return ret;
}

static void audio_prompt_tone_detach_speaker_port(audio_prompt_tone_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

#if CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE
    if (ctx->spk_stream != NULL) {
        audio_port_info_t port_info = {0};
        port_info.port_id = ctx->port_id;
        port_info.priority = AUDIO_SPK_PORT_PRIORITY_TONE;
        port_info.port = NULL;
        onboard_speaker_stream_set_input_port_info(ctx->spk_stream, &port_info);
    }
#endif

    if (ctx->player_handle != NULL) {
        bk_player_set_output_port(ctx->player_handle, NULL);
    }
}

audio_prompt_tone_handle_t audio_prompt_tone_init(audio_prompt_tone_cfg_t *cfg)
{
    audio_prompt_tone_ctx_t *ctx = NULL;

    if (cfg == NULL || cfg->spk_stream == NULL) {
        LOGE("cfg is NULL");
        return NULL;
    }

    LOGD("port_id: %d, rb_size: %d, dec_type: %d\r\n", cfg->port_id, cfg->rb_size, cfg->dec_type);

    ctx = psram_malloc(sizeof(audio_prompt_tone_ctx_t));
    if (ctx == NULL) {
        LOGE("malloc failed");
        return NULL;
    }
    os_memset(ctx, 0x00, sizeof(audio_prompt_tone_ctx_t));

    // 拷贝配置参数
    ctx->port_id = cfg->port_id;
    ctx->rb_size = cfg->rb_size;
    ctx->dec_type = cfg->dec_type;
    ctx->spk_stream = cfg->spk_stream;

    if (audio_prompt_tone_create_player(ctx) != BK_OK) {
        goto error;
    }

    if (audio_prompt_tone_create_output_port(ctx) != BK_OK) {
        goto error;
    }

    LOGD("spk_stream: %p, out_port: %p\r\n", ctx->spk_stream, ctx->player_out_port);
    
    LOGD("audio prompt tone init success\r\n");

    return ctx;

error:
    if (ctx->player_handle) {
        bk_player_destroy(ctx->player_handle);
    }

    if (ctx->player_out_port) {
        audio_port_deinit(ctx->player_out_port);
        ctx->player_out_port = NULL;
    }

    if (ctx) {
        psram_free(ctx);
        ctx = NULL;
    }

    return NULL;
}

bk_err_t audio_prompt_tone_start(audio_prompt_tone_handle_t handle, audio_prompt_tone_info_t *info)
{
    bk_err_t ret = BK_OK;
    audio_prompt_tone_ctx_t *ctx = (audio_prompt_tone_ctx_t *)handle;

    if (handle == NULL || info == NULL || info->data == NULL) {
        LOGE("invalid param");
        return BK_FAIL;
    }

    if (ctx->player_handle != NULL) {
        ret = audio_prompt_tone_stop(handle);
        if (ret != BK_OK) {
            LOGW("stop current prompt tone before start failed, ret:%d\r\n", ret);
        }
    }

    ret = audio_prompt_tone_create_player(ctx);
    if (ret != BK_OK) {
        return ret;
    }

    ret = audio_prompt_tone_create_output_port(ctx);
    if (ret != BK_OK) {
        return ret;
    }

    player_uri_info_t player_uri_info = {0};
    player_uri_info.uri_type = info->uri_type;
    player_uri_info.uri = info->data;
    player_uri_info.total_len = info->len;

    if (info->uri_type == PLAYER_URI_TYPE_ARRAY) {
        bk_player_set_decode_type(ctx->player_handle, ctx->dec_type);
    }

    ret = bk_player_set_uri(ctx->player_handle, &player_uri_info);
    if (BK_OK != ret) {
        LOGE("bk_player_set_uri fail, ret: %d\r\n", ret);
        return ret;
    }

    ret = bk_player_set_output_port(ctx->player_handle, ctx->player_out_port);
    if (BK_OK != ret) {
        LOGE("bk_player_set_output_port fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    ret = bk_player_start(ctx->player_handle);
    if (BK_OK != ret) {
        LOGE("bk_player_start fail, ret: %d\r\n", ret);
        return ret;
    }
    ctx->playback_finished = false;

    return BK_OK;
}

bk_err_t audio_prompt_tone_stop(audio_prompt_tone_handle_t handle)
{
    bk_err_t ret = BK_OK;
    audio_prompt_tone_ctx_t *ctx = (audio_prompt_tone_ctx_t *)handle;

    if (handle == NULL) {
        LOGE("handle is NULL");
        return BK_FAIL;
    }

    if (ctx->player_handle) {
        if (!ctx->playback_finished) {
            LOGD("stop active prompt tone before destroy\r\n");
        }
        ret = audio_prompt_tone_destroy_player(ctx, !ctx->playback_finished);
        if (ret != BK_OK) {
            LOGW("destroy prompt tone player failed, ret:%d\r\n", ret);
        }
    }

    if (ctx->player_out_port) {
        ret = audio_port_deinit(ctx->player_out_port);
        if (ret != BK_OK) {
            LOGW("audio_port_deinit failed, ret:%d\r\n", ret);
        }
        ctx->player_out_port = NULL;
    }

    ret = audio_prompt_tone_create_player(ctx);
    if (ret != BK_OK) {
        return ret;
    }

    ret = audio_prompt_tone_create_output_port(ctx);
    if (ret != BK_OK) {
        return ret;
    }

    return BK_OK;
}

bk_err_t audio_prompt_tone_deinit(audio_prompt_tone_handle_t handle)
{
    audio_prompt_tone_ctx_t *ctx = (audio_prompt_tone_ctx_t *)handle;

    if (handle == NULL) {
        LOGE("handle is NULL");
        return BK_FAIL;
    }

    if (ctx->player_handle) {
        audio_prompt_tone_destroy_player(ctx, !ctx->playback_finished);
    }

    if (ctx->player_out_port) {
        audio_port_deinit(ctx->player_out_port);
        ctx->player_out_port = NULL;
    }

    psram_free(ctx);

    LOGD("audio prompt tone deinit success\r\n");

    return BK_OK;
}
