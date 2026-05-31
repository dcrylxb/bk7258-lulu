#include <os/mem.h>
#include <components/log.h>
#include <components/bk_audio_player/bk_audio_player.h>
#include <components/bk_audio/audio_pipeline/rb_port.h>
#include <components/bk_audio/audio_streams/onboard_speaker_stream.h>

// Sources
#include <components/bk_audio_player/plugins/sources/bk_audio_player_file_source.h>
#include <components/bk_audio_player/plugins/sources/bk_audio_player_net_source.h>
#include <components/bk_audio_player/plugins/sources/bk_audio_player_hls_source.h>

// Decoders
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_mp3_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_wav_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_aac_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_flac_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_ogg_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_opus_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_m4a_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_amr_decoder.h>
#include <components/bk_audio_player/plugins/decoders/bk_audio_player_ts_decoder.h>

// Metadata parsers
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_mp3_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_wav_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_aac_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_flac_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_ogg_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_opus_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_m4a_metadata_parser.h>
#include <components/bk_audio_player/plugins/metadata_parsers/bk_audio_player_amr_metadata_parser.h>

#include "app_audio_player.h"
#include "bk_audio_player_port_sink.h"

#define TAG "audio"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

typedef struct {
    bool                         inited;
    bk_audio_player_handle_t      player_handle;
    audio_port_handle_t           rb_port;
    audio_element_handle_t        spk_stream;
    uint8_t                      port_id;
    audio_player_event_handler_func user_event_handler;
    void                         *user_data;
} app_audio_player_ctx_t;

static bk_audio_player_handle_t g_player_handle = NULL;

static bk_err_t _register_default_plugins(bk_audio_player_handle_t player, app_audio_player_cfg_t *cfg)
{
    bk_err_t ret;

    // Register default sources
    ret = bk_audio_player_register_source(player, bk_audio_player_get_file_source_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register file source fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    ret = bk_audio_player_register_source(player, bk_audio_player_get_net_source_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register net source fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    // ret = bk_audio_player_register_source(player, bk_audio_player_get_hls_source_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register hls source fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // Register default decoders
    ret = bk_audio_player_register_decoder(player, bk_audio_player_get_mp3_decoder_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register mp3 decoder fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    ret = bk_audio_player_register_decoder(player, bk_audio_player_get_wav_decoder_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register wav decoder fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_aac_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register aac decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_flac_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register flac decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_ogg_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register ogg decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_opus_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register opus decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_m4a_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register m4a decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_amr_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register amr decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_decoder(player, bk_audio_player_get_ts_decoder_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register ts decoder fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // Register default metadata parsers
    ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_mp3_metadata_parser_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register mp3 metadata parser fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_wav_metadata_parser_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register wav metadata parser fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_aac_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register aac metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_flac_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register flac metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_ogg_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register ogg metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_opus_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register opus metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_m4a_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register m4a metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // ret = bk_audio_player_register_metadata_parser(player, bk_audio_player_get_amr_metadata_parser_ops());
    // if (ret != AUDIO_PLAYER_OK) {
    //     LOGE("register amr metadata parser fail, ret: %d\r\n", ret);
    //     return BK_FAIL;
    // }

    // Register port sink
    bk_audio_player_port_sink_param_t port_param = {0};
    port_param.port_id = cfg->port_id;
    port_param.priority = cfg->priority;
    port_param.spk_str = cfg->spk_stream;
    port_param.rb_port = cfg->rb_port;
    ret = bk_audio_player_set_port_sink_param(&port_param);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("set port sink param fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    ret = bk_audio_player_register_sink(player, bk_audio_player_get_port_sink_ops());
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("register port sink fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    LOGD("default plugins registered success\r\n");

    return BK_OK;
}

bk_err_t app_audio_player_init(app_audio_player_cfg_t *cfg)
{
    bk_err_t ret = BK_OK;

    if (g_player_handle) {
        LOGW("app_audio_player already initialized\r\n");
        return BK_OK;
    }

    if (!cfg || !cfg->spk_stream) {
        LOGE("cfg is NULL\r\n");
        return BK_ERR_PARAM;
    }

    if (cfg->port_id < 1) {
        LOGE("port_id must be >= 1, got %d\r\n", cfg->port_id);
        return BK_ERR_PARAM;
    }

    LOGD("port_id: %d, priority: %d, rb_port: %p, spk_stream: %p\r\n",
        cfg->port_id, cfg->priority, cfg->rb_port, cfg->spk_stream);

    bk_audio_player_cfg_t player_cfg = DEFAULT_AUDIO_PLAYER_CONFIG();
    player_cfg.event_handler = cfg->event_handler;
    player_cfg.args = cfg->user_data;

    ret = bk_audio_player_new(&g_player_handle, &player_cfg);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_new fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    // Register all default plugins
    ret = _register_default_plugins(g_player_handle, cfg);
    if (ret != BK_OK) {
        LOGE("register default plugins fail\r\n");
        bk_audio_player_delete(g_player_handle);
        g_player_handle = NULL;
        return BK_FAIL;
    }

    LOGD("app_audio_player init success\r\n");

    return BK_OK;
}

bk_err_t app_audio_player_deinit(void)
{
    if (!g_player_handle) {
        LOGW("app_audio_player not initialized\r\n");
        return BK_OK;
    }

    bk_audio_player_stop(g_player_handle);
    bk_audio_player_delete(g_player_handle);
    g_player_handle = NULL;

    LOGD("app_audio_player deinit success\r\n");

    return BK_OK;
}

bk_err_t app_audio_player_add_music(char *name, char *uri)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    if (!name || !uri) {
        LOGE("name or uri is NULL\r\n");
        return BK_ERR_PARAM;
    }

    ret = bk_audio_player_add_music(g_player_handle, name, uri);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_add_music fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_clear_music_list(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_clear_music_list(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_clear_music_list fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_start(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_start(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_start fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_stop(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_stop(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_stop fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_pause(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_pause(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_pause fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_resume(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_resume(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_resume fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_next(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_next(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_next fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_prev(void)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_prev(g_player_handle);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_prev fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_seek(int second)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    ret = bk_audio_player_seek(g_player_handle, second);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_seek fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t app_audio_player_set_volume(int volume)
{
    int ret;

    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return BK_FAIL;
    }

    if (volume < 0 || volume > 100) {
        LOGE("volume %d is out of range (0-100)\r\n", volume);
        return BK_ERR_PARAM;
    }

    ret = bk_audio_player_set_volume(g_player_handle, volume);
    if (ret != AUDIO_PLAYER_OK) {
        LOGE("bk_audio_player_set_volume fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    return BK_OK;
}

int app_audio_player_get_volume(void)
{
    if (!g_player_handle) {
        LOGE("app_audio_player not initialized\r\n");
        return -1;
    }

    return bk_audio_player_get_volume(g_player_handle);
}