// Copyright 2024-2025 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os/mem.h>
#include <os/os.h>
#include <components/log.h>
#include <components/bk_audio_player/bk_audio_player.h>
#include <components/bk_audio/audio_streams/onboard_speaker_stream.h>
#include "plugin_manager.h"

#include "bk_audio_player_port_sink.h"

#define TAG "port_sink"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static bk_audio_player_port_sink_param_t g_port_sink_param;

#define PORT_SINK_DRAIN_POLL_MS         (60)
#define PORT_SINK_DRAIN_WAIT_MS         (10 * PORT_SINK_DRAIN_POLL_MS)

static int _audio_port_state_notify_cb(int state, void *port_info, void *user_data)
{
    bk_audio_player_handle_t player = (bk_audio_player_handle_t)user_data;

    LOGI("audio port state: %d\r\n", state);

    if (!player) {
        return BK_FAIL;
    }

    switch (state) {
    case APT_STATE_RUNNING:
        // bk_audio_player_resume(player);
        break;
    case APT_STATE_PAUSED:
        // bk_audio_player_pause(player);
        break;
    case APT_STATE_FINISHED:
        break;
    default:
        break;
    }

    return BK_OK;
}

static void port_sink_wait_drain_done(audio_port_handle_t rb_port)
{
    uint32_t start_ms = 0;

    if (!rb_port) {
        return;
    }

    start_ms = rtos_get_time();

    LOGI("drain start, filled=%u\r\n", audio_port_get_filled_size(rb_port));

    while ((rtos_get_time() - start_ms) < PORT_SINK_DRAIN_WAIT_MS) {
        if (audio_port_get_filled_size(rb_port) == 0) {
            rtos_delay_milliseconds(PORT_SINK_DRAIN_POLL_MS);
            if (audio_port_get_filled_size(rb_port) == 0) {
                LOGI("drain finished\r\n");
                return;
            }
        }

        rtos_delay_milliseconds(PORT_SINK_DRAIN_POLL_MS);
    }

    LOGW("drain timeout, filled=%u\r\n", audio_port_get_filled_size(rb_port));
}

static int port_sink_open(audio_sink_type_t sink_type,
                           void *param,
                           bk_audio_player_sink_t **sink_pp)
{
    bk_audio_player_sink_t *sink = NULL;
    bk_audio_player_port_sink_param_t *port_param = &g_port_sink_param;
    device_sink_param_t *dev_param = NULL;

    LOGI("port_sink_open\r\n");

    if (sink_type != AUDIO_SINK_DEVICE) {
        LOGE("invalid sink type %d\r\n", sink_type);
        return AUDIO_PLAYER_INVALID;
    }

    dev_param = (device_sink_param_t *)param;
    if (!dev_param) {
        LOGE("dev_param is NULL\r\n");
        return AUDIO_PLAYER_INVALID;
    }

    if (!port_param->spk_str) {
        LOGE("spk_str is NULL\r\n");
        return AUDIO_PLAYER_INVALID;
    }

    if (!port_param->rb_port) {
        LOGE("rb_port is NULL\r\n");
        return AUDIO_PLAYER_INVALID;
    }

    if (port_param->port_id < 1) {
        LOGE("invalid port_id %d, must be >= 1\r\n", port_param->port_id);
        return AUDIO_PLAYER_INVALID;
    }

    LOGI("port_id=%d, priority=%d, spk_str=%p, rb_port=%p\r\n",
            port_param->port_id, port_param->priority, port_param->spk_str, port_param->rb_port);

    sink = audio_sink_new(sizeof(bk_audio_player_port_sink_param_t));
    if (!sink) {
        LOGE("audio_sink_new failed\r\n");
        return AUDIO_PLAYER_NO_MEM;
    }

    bk_audio_player_port_sink_param_t *priv = (bk_audio_player_port_sink_param_t *)sink->sink_priv;
    os_memcpy(priv, port_param, sizeof(bk_audio_player_port_sink_param_t));

    if (audio_port_reset(priv->rb_port) != BK_OK) {
        LOGE("audio_port_reset failed\r\n");
        player_free(sink);
        return AUDIO_PLAYER_ERR;
    }

    // 注册端口信息到 spk_str
#if CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE
    bk_audio_player_handle_t player = dev_param->player;
    int gain = player->spk_gain * 63 / 100;
    audio_port_info_t port_info = DEFAULT_AUDIO_PORT_INFO();
    port_info.chl_num = dev_param->info->channel_number;
    port_info.sample_rate = dev_param->info->sample_rate;
    port_info.bits = dev_param->info->sample_bits;
    port_info.dig_gain = gain;
    port_info.port_id = priv->port_id;
    port_info.priority = priv->priority;
    port_info.port = priv->rb_port;
    port_info.notify_cb = _audio_port_state_notify_cb;
    port_info.user_data = (void *)player;
    if (onboard_speaker_stream_set_input_port_info(priv->spk_str, &port_info) != BK_OK) {
        LOGE("onboard_speaker_stream_set_input_port_info failed\r\n");
        player_free(sink);
        return AUDIO_PLAYER_ERR;
    }
#else
    LOGE("CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE not defined\r\n");
    player_free(sink);
    return AUDIO_PLAYER_ERR;
#endif

    *sink_pp = sink;

    LOGI("port_sink_open success\r\n");

    return AUDIO_PLAYER_OK;
}

static int port_sink_close(bk_audio_player_sink_t *sink)
{
    LOGI("port_sink_close\r\n");

    if (!sink) {
        return AUDIO_PLAYER_INVALID;
    }

    bk_audio_player_port_sink_param_t *priv = (bk_audio_player_port_sink_param_t *)sink->sink_priv;

#if CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE
    int ret = audio_port_write_done(priv->rb_port);
    if (ret != BK_OK) {
        LOGW("audio_port_write_done failed, ret: %d\r\n", ret);
    }
    port_sink_wait_drain_done(priv->rb_port);

    // 清空 spk_str 的输入端口信息
    audio_port_info_t port_info = {0};
    port_info.port_id = priv->port_id;
    port_info.port = NULL;
    onboard_speaker_stream_set_input_port_info(priv->spk_str, &port_info);
#endif

    LOGI("port_sink_close success\r\n");

    return AUDIO_PLAYER_OK;
}

static int port_sink_write(bk_audio_player_sink_t *sink, char *buffer, int len)
{
    bk_audio_player_port_sink_param_t *priv = NULL;
    int ret;

    if (!sink || !buffer || len <= 0) {
        return -1;
    }

    priv = (bk_audio_player_port_sink_param_t *)sink->sink_priv;

    ret = audio_port_write(priv->rb_port, (char *)buffer, len, portMAX_DELAY);
    if (ret != len) {
        LOGW("write audio to ringbuffer incomplete, written: %d, expected: %d\n", ret, len);
    }

    return ret;
}

static int port_sink_control(bk_audio_player_sink_t *sink, audio_sink_control_t control)
{
    int ret = BK_OK;
    bk_audio_player_port_sink_param_t *priv = NULL;

    if (!sink) {
        return AUDIO_PLAYER_INVALID;
    }

    priv = (bk_audio_player_port_sink_param_t *)sink->sink_priv;

    switch (control) {
#if CONFIG_ADK_ONBOARD_SPEAKER_STREAM_SUPPORT_MULTIPLE_SOURCE
        case AUDIO_SINK_PAUSE:
        {
            LOGI("paused port_id=%d\r\n", priv->port_id);
            break;
        }
        case AUDIO_SINK_RESUME:
        {
            LOGI("resumed port_id=%d\r\n", priv->port_id);
            break;
        }
        case AUDIO_SINK_FRAME_INFO_CHANGE:
        {
            uint8_t dig_gain = 0;
            uint8_t ana_gain = 0;
            onboard_speaker_stream_get_digital_gain(priv->spk_str, &dig_gain);
            onboard_speaker_stream_get_analog_gain(priv->spk_str, &ana_gain);

            LOGI("sample_rate: %d, bits: %d, chl_num: %d\r\n",
                 sink->info.sampRate, sink->info.bitsPerSample, sink->info.nChans);

            audio_port_info_t port_info = DEFAULT_AUDIO_PORT_INFO();
            port_info.chl_num = sink->info.nChans;
            port_info.sample_rate = sink->info.sampRate;
            port_info.bits = sink->info.bitsPerSample;
            port_info.dig_gain = dig_gain;
            port_info.ana_gain = ana_gain;
            port_info.port_id = priv->port_id;
            port_info.priority = priv->priority;
            port_info.port = priv->rb_port;
            port_info.notify_cb = _audio_port_state_notify_cb;
            ret = onboard_speaker_stream_set_input_port_info(priv->spk_str, &port_info);
            if (ret != BK_OK) {
                LOGE("onboard_speaker_stream_set_input_port_info fail, ret: %d\r\n", ret);
            }
            break;
        }
        case AUDIO_SINK_SET_VOLUME:
        {
            ret = onboard_speaker_stream_set_digital_gain(priv->spk_str, sink->info.volume);
            if (ret != BK_OK) {
                LOGE("set volume fail, ret: %d\r\n", ret);
            } else {
                LOGI("set volume to %d\r\n", sink->info.volume);
            }
            break;
        }
#endif
        default:
            break;
    }

    return AUDIO_PLAYER_OK;
}

static const bk_audio_player_sink_ops_t port_sink_ops = {
    .open    = port_sink_open,
    .write   = port_sink_write,
    .control = port_sink_control,
    .close   = port_sink_close,
};

const bk_audio_player_sink_ops_t *bk_audio_player_get_port_sink_ops(void)
{
    return &port_sink_ops;
}

int bk_audio_player_set_port_sink_param(bk_audio_player_port_sink_param_t *param)
{
    if (!param) {
        return AUDIO_PLAYER_INVALID;
    }

    os_memcpy(&g_port_sink_param, param, sizeof(bk_audio_player_port_sink_param_t));

    return AUDIO_PLAYER_OK;
}
