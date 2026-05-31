#include <stdbool.h>
#include <stdint.h>

#include <components/bk_voice_service.h>
#include <components/bk_voice_read_service.h>
#include <components/bk_voice_write_service.h>
#include <components/bk_audio/audio_streams/onboard_speaker_stream.h>
#include <os/mem.h>
#include <os/os.h>
#include <common/bk_include.h>
#include <components/log.h>
#include <driver/gpio.h>
#include "gpio_driver.h"
#include <driver/hal/hal_gpio_types.h>
#include "common.h"

#if CONFIG_ASR_SERVICE
#include <components/bk_asr_service.h>
#include <components/bk_audio_asr_service.h>
#include "bk_app_asr.h"
#endif

#include "bk_app_audio.h"

#if CONFIG_PLAYER_SERVICE
#include "audio_prompt_tone.h"
#endif

#if CONFIG_AUDIO_PLAYER
#include "app_audio_player.h"
#endif

#if CONFIG_AUDIO_PARA
#include "bk_app_audio_param.h"
#endif

#define TAG "audio"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define OPUS_DEC_STACK_SIZE   (12 * 1024)
#define OPUS_DEC_FRAME_SIZE   (480)          // 32kbps 码率，120ms 间隔
#define SPK_FRAME_NUM         (16)           // 缓存 16 帧

#define SPK_GAIN_MAX          0x3F           // [0x00, 0x3F]
#define MIC_DIG_GAIN_DEFAULT  0x30           // 0x2D is 0 dB
#define MIC_ANA_GAIN_DEFAULT  0x0A           // 4-bit analog mic gain

#define VAD_ENG_THRESHOLD     3000

typedef struct {
    bool                  inited;            // 初始化标志
    voice_handle_t        voice_handle;      // 语音服务句柄
    voice_read_handle_t   voice_read_handle; // 录音句柄
    voice_write_handle_t  voice_write_handle;// 播放句柄
#if CONFIG_ASR_SERVICE
    asr_handle_t          asr_handle;        // ASR服务句柄
    aud_asr_handle_t      aud_asr_handle;    // ASR句柄
#endif
#if CONFIG_PLAYER_SERVICE
    bool                  prompt_tone_enable; // 提示音使能标志
    audio_prompt_tone_handle_t prompt_tone_handle; // 提示音句柄
#endif
#if CONFIG_AUDIO_PLAYER
    bool                  audio_player_enable; // Audio player使能标志
    audio_port_handle_t   audio_player_port_handle;
#endif
    uint16_t              pa_ctrl_gpio;      // PA控制GPIO
    uint8_t               pa_on_level;       // PA开启电平
} app_voice_ctx_t;

static app_voice_ctx_t g_voice_ctx = {0};

static void _dump_config(app_audio_config_t *config)
{
    LOGI("=== Voice Configuration Parameters ===\n");
    LOGI("AEC enable: %s\n", config->aec_enable ? "true" : "false");
    LOGI("AEC mode: %s\n", config->aec_mode == AEC_MODE_HARDWARE ? "hardware" : "software");
    LOGI("Decoder type: %d\n", config->dec_type);
    LOGI("Speaker sample rate: %d Hz\n", config->spk_sample_rate);
    LOGI("Speaker frame duration: %d ms\n", config->spk_frame_duration);
    LOGI("PA control GPIO: %d\n", config->pa_ctrl_gpio);
    LOGI("PA on level: %d\n", config->pa_on_level);
    LOGI("Encoder type: %d\n", config->enc_type);
    LOGI("MIC sample rate: %d Hz\n", config->mic_sample_rate);
    LOGI("MIC gains: digital=0x%02x analog=0x%02x\n", MIC_DIG_GAIN_DEFAULT, MIC_ANA_GAIN_DEFAULT);
    LOGI("======================================\n");
}

#if CONFIG_AUDIO_PARA
static void _update_voice_cfg(app_aud_service_type_t service_type, voice_cfg_t *voice_cfg)
{
    app_aud_para_t *aud_para = NULL;

    if (service_type != AUD_SERVICE_AI_VOC) {
        return;
    }

    aud_para = bk_app_audio_get_cust_para(service_type);
    if (aud_para == NULL) {
        LOGE("cust para is NULL\n");
        return;
    }

#if CONFIG_VOICE_SERVICE_EQ
    if (voice_cfg->eq_en) {
        voice_cfg->eq_cfg.eq_alg_cfg.eq_cal_para.eq_en       = aud_para->eq_dl_config.eq_en;
        voice_cfg->eq_cfg.eq_alg_cfg.eq_cal_para.filters     = aud_para->eq_dl_config.filters;
        voice_cfg->eq_cfg.eq_alg_cfg.eq_cal_para.globle_gain = aud_para->eq_dl_config.globle_gain;
        os_memcpy(&voice_cfg->eq_cfg.eq_alg_cfg.eq_cal_para.eq_para, &aud_para->eq_dl_config.eq_para, sizeof(eq_para_t) * aud_para->eq_dl_config.filters);
        os_memcpy(&voice_cfg->eq_cfg.eq_alg_cfg.eq_cal_para.eq_load, &aud_para->eq_dl_config.eq_load, sizeof(app_eq_load_t));
    }
#endif

    if (voice_cfg->mic_type == MIC_TYPE_ONBOARD && aud_para->sys_config.app_sys_en) {
        voice_cfg->mic_cfg.onboard_mic_cfg.adc_cfg.ana_gain = aud_para->sys_config.mic0_analog_gain;
        voice_cfg->mic_cfg.onboard_mic_cfg.adc_cfg.dig_gain = aud_para->sys_config.mic0_digital_gain;
    }

    if (voice_cfg->spk_type == SPK_TYPE_ONBOARD && aud_para->sys_config.app_sys_en) {
        voice_cfg->spk_cfg.onboard_spk_cfg.ana_gain = aud_para->sys_config.speaker_chan0_analog_gain;
        voice_cfg->spk_cfg.onboard_spk_cfg.dig_gain = aud_para->sys_config.speaker_chan0_digital_gain;
    }

    if (voice_cfg->aec_en && aud_para->aec_v3_config.app_aec_en) {
        voice_cfg->aec_cfg.aec_alg_cfg.aec_cfg.delay_points = aud_para->aec_v3_config.mic_delay;
        voice_cfg->aec_cfg.aec_alg_cfg.aec_cfg.ec_depth     = aud_para->aec_v3_config.ec_depth;
        voice_cfg->aec_cfg.aec_alg_cfg.aec_cfg.ref_scale    = aud_para->aec_v3_config.ref_scale;
        voice_cfg->aec_cfg.aec_alg_cfg.aec_cfg.ns_level     = aud_para->aec_v3_config.ns_level;
        voice_cfg->aec_cfg.aec_alg_cfg.aec_cfg.ns_para      = aud_para->aec_v3_config.ns_para;
    }
}
#endif

bk_err_t bk_app_audio_init(app_audio_config_t *config)
{
    uint32_t frame_size = 0;

    if (!config) {
        LOGE("config is NULL\n");
        return BK_ERR_PARAM;
    }

    if (g_voice_ctx.inited) {
        LOGW("app voice service already initialized\n");
        return BK_OK;
    }

    if (config->mic_sample_rate != 16000 && config->mic_sample_rate != 8000) {
        LOGE("mic sample rate %d not supported\n", config->mic_sample_rate);
        return BK_ERR_PARAM;
    }

    _dump_config(config);

    voice_cfg_t voice_cfg = VOICE_BY_ONBOARD_MIC_SPK_CFG_DEFAULT();

    frame_size = config->mic_sample_rate * 2 * MIC_FRAME_DURATION / 1000;

    // 配置麦克风
    voice_cfg.mic_cfg.onboard_mic_cfg.adc_cfg.sample_rate = config->mic_sample_rate;
    voice_cfg.mic_cfg.onboard_mic_cfg.adc_cfg.dig_gain = MIC_DIG_GAIN_DEFAULT;
    voice_cfg.mic_cfg.onboard_mic_cfg.adc_cfg.ana_gain = MIC_ANA_GAIN_DEFAULT;
    voice_cfg.mic_cfg.onboard_mic_cfg.frame_size = frame_size;
    voice_cfg.mic_cfg.onboard_mic_cfg.out_block_size = voice_cfg.mic_cfg.onboard_mic_cfg.frame_size;
    voice_cfg.mic_cfg.onboard_mic_cfg.out_block_num = 2;

    // 配置扬声器
    voice_cfg.spk_cfg.onboard_spk_cfg.sample_rate = config->spk_sample_rate;
    voice_cfg.spk_cfg.onboard_spk_cfg.frame_size = config->spk_sample_rate * 2 * config->spk_frame_duration / 1000;
    voice_cfg.spk_cfg.onboard_spk_cfg.dig_gain = 0x1F;
    voice_cfg.spk_cfg.onboard_spk_cfg.multi_in_port_num = 3;

    if (config->pa_ctrl_gpio < GPIO_NUM) {
        LOGI("enable PA control, GPIO: %d, level: %d\n", config->pa_ctrl_gpio, config->pa_on_level);
        voice_cfg.spk_cfg.onboard_spk_cfg.pa_ctrl_en = true;
        voice_cfg.spk_cfg.onboard_spk_cfg.pa_ctrl_gpio = config->pa_ctrl_gpio;
        voice_cfg.spk_cfg.onboard_spk_cfg.pa_on_level = config->pa_on_level;
        voice_cfg.spk_cfg.onboard_spk_cfg.pa_on_delay = 10;
        voice_cfg.spk_cfg.onboard_spk_cfg.pa_off_delay = 30;
    }
    g_voice_ctx.pa_ctrl_gpio = config->pa_ctrl_gpio;
    g_voice_ctx.pa_on_level = config->pa_on_level;

    // 配置AEC
    voice_cfg.aec_en = config->aec_enable;
    if (config->aec_enable) {
        voice_cfg.aec_cfg.aec_alg_cfg.task_stack = 2048;
        voice_cfg.aec_cfg.aec_alg_cfg.aec_cfg.ns_type = NS_TRADITION;
        voice_cfg.aec_cfg.aec_alg_cfg.aec_cfg.fs = config->mic_sample_rate;
        voice_cfg.aec_cfg.aec_alg_cfg.out_block_size = frame_size;
        if (config->aec_mode == AEC_MODE_HARDWARE) { // 硬件回采，mic 配置成双通道，左通道是原始数据，右声道是参考数据，同时 spk 不需要多路输出
            voice_cfg.aec_cfg.aec_alg_cfg.aec_cfg.mode = AEC_MODE_HARDWARE;
            voice_cfg.mic_cfg.onboard_mic_cfg.adc_cfg.chl_num = 2;
            voice_cfg.mic_cfg.onboard_mic_cfg.multi_out_port_num = 1;
            voice_cfg.spk_cfg.onboard_spk_cfg.multi_out_port_num = 0;
        } else { // 软件回采，mic 配置成单通道，spk 配置多路输出
            voice_cfg.aec_cfg.aec_alg_cfg.aec_cfg.mode = AEC_MODE_SOFTWARE;
            voice_cfg.mic_cfg.onboard_mic_cfg.adc_cfg.chl_num = 1;
            voice_cfg.spk_cfg.onboard_spk_cfg.multi_out_port_num = 1;
        }

        // 配置VAD
#if CONFIG_VAD
        voice_cfg.aec_cfg.aec_alg_cfg.vad_cfg.vad_enable = config->vad_enable;
        if (config->vad_enable) {
            voice_cfg.aec_cfg.aec_alg_cfg.vad_cfg.vad_eng_threshold = VAD_ENG_THRESHOLD;
            voice_cfg.aec_cfg.aec_alg_cfg.vad_cfg.vad_buf_size = 0;
            voice_cfg.aec_cfg.aec_alg_cfg.vad_cfg.vad_frame_size = 0;
            voice_cfg.aec_cfg.aec_alg_cfg.vad_state_cb = config->vad_state_cb;
            voice_cfg.enc_common.frame_in_ms = MIC_FRAME_DURATION;
            voice_cfg.enc_common.frame_in_size = frame_size;
        }
#endif
    }

    // 配置编码器
    switch (config->enc_type) {
        case AUDIO_ENC_TYPE_OPUS:
        {
            opus_enc_cfg_t opus_enc_cfg = DEFAULT_OPUS_ENC_CONFIG();
            opus_enc_cfg.frame_samples_per_channel = config->mic_sample_rate * MIC_FRAME_DURATION / 1000;
            voice_cfg.enc_type = AUDIO_ENC_TYPE_OPUS;
            voice_cfg.enc_cfg.opus_enc_cfg = opus_enc_cfg;

        }
        break;

        case AUDIO_ENC_TYPE_PCM:
        {
            voice_cfg.enc_type = AUDIO_ENC_TYPE_PCM;
            voice_cfg.read_pool_size = frame_size;
        }
        break;

        default:
            LOGE("unsupported encoder type: %d\n", config->enc_type);
            return BK_FAIL;
    }

    // 配置解码器
    switch (config->dec_type) {
        case AUDIO_DEC_TYPE_OPUS:
        {
            opus_dec_cfg_t opus_dec_cfg = DEFAULT_OPUS_DEC_CONFIG();
            opus_dec_cfg.task_stack = OPUS_DEC_STACK_SIZE;
            voice_cfg.dec_type = AUDIO_DEC_TYPE_OPUS;
            voice_cfg.dec_cfg.opus_dec_cfg = opus_dec_cfg;
        }
        break;

        case AUDIO_DEC_TYPE_PCM:
        {
            voice_cfg.dec_type = AUDIO_DEC_TYPE_PCM;
            voice_cfg.write_pool_size = config->spk_sample_rate * 2 * config->spk_frame_duration / 1000;
        }
        break;

        default:
            LOGE("unsupported decoder type: %d\n", config->dec_type);
            return BK_FAIL;
    }

#if CONFIG_VOICE_SERVICE_EQ
    // 配置均衡器
    voice_cfg.eq_en = true;
    eq_algorithm_cfg_t eq_cfg = DEFAULT_EQ_ALGORITHM_CONFIG();
    voice_cfg.eq_cfg.eq_alg_cfg = eq_cfg;
#endif

#if CONFIG_ASR_SERVICE
    // 配置ASR
    asr_cfg_t asr_cfg = ASR_BY_ONBOARD_MIC_CFG_DEFAULT();
    asr_cfg.asr_en = true;
    asr_cfg.read_pool_size = asr_cfg.asr_sample_rate * 2 * 20 / 1000; // 20ms帧大小
    
    // 如果麦克风采样率与ASR采样率不同，需要启用重采样
    if (config->mic_sample_rate != asr_cfg.asr_sample_rate) {
        asr_cfg.asr_rsp_en = true;
        asr_cfg.rsp_cfg.rsp_alg_cfg.rsp_cfg.src_rate = config->mic_sample_rate;
        asr_cfg.rsp_cfg.rsp_alg_cfg.rsp_cfg.dest_rate = asr_cfg.asr_sample_rate;
    } else {
        asr_cfg.asr_rsp_en = false;
    }
    
    // 配置多输出端口，若使能 AEC，从 AEC Element 输出，否则从麦克风 Element 输出
    if (config->aec_enable) {
        voice_cfg.aec_cfg.aec_alg_cfg.multi_out_port_num++;
    } else {
        voice_cfg.mic_cfg.onboard_mic_cfg.multi_out_port_num++;
    }
#endif

    // 设置事件回调
    voice_cfg.event_handle = config->event_cb;
    voice_cfg.args = config->user_data;

#if CONFIG_AUDIO_PARA
    _update_voice_cfg(AUD_SERVICE_AI_VOC, &voice_cfg);
    bk_app_audio_param_init(AUD_SERVICE_AI_VOC);
#endif

    // 初始化语音服务
    g_voice_ctx.voice_handle = bk_voice_init(&voice_cfg);
    if (!g_voice_ctx.voice_handle) {
        LOGE("voice init failed\n");
        goto error;
    }

#if CONFIG_ASR_SERVICE
    g_voice_ctx.asr_handle = bk_asr_create(&asr_cfg);
    if (!g_voice_ctx.asr_handle) {
        LOGE("asr create failed\n");
        goto error;
    }

    // 关联多输出端口
    g_voice_ctx.asr_handle->mic_str = (audio_element_handle_t)bk_voice_get_mic_str(g_voice_ctx.voice_handle, &voice_cfg);

    // 初始化ASR
    if (BK_OK != bk_asr_init(&asr_cfg, g_voice_ctx.asr_handle)) {
        LOGE("asr init failed\n");
        goto error;
    }

    aud_asr_cfg_t aud_asr_cfg = AUDIO_ASR_CFG_DEFAULT();
    aud_asr_cfg.asr_handle = g_voice_ctx.asr_handle;
    if (config->asr_frame_size > 0) {
        LOGI("asr frame size: %d", config->asr_frame_size);
        aud_asr_cfg.max_read_size = config->asr_frame_size;
    }
    aud_asr_cfg.task_prio = voice_cfg.mic_cfg.onboard_mic_cfg.task_prio;
    aud_asr_cfg.aud_asr_init = bk_app_asr_init;
    aud_asr_cfg.aud_asr_deinit = bk_app_asr_deinit;
    aud_asr_cfg.aud_asr_recog = bk_app_asr_recog;
    aud_asr_cfg.aud_asr_result_handle = config->asr_result_cb;

    if (aud_asr_cfg.aud_asr_result_handle == NULL) {
        LOGW("asr result callback is NULL\n");
    }
    
    g_voice_ctx.aud_asr_handle = bk_aud_asr_init(&aud_asr_cfg);
    if (!g_voice_ctx.aud_asr_handle) {
        LOGE("aud asr init failed\n");
        goto error;
    }
#endif

#if CONFIG_AUDIO_PARA
    bk_app_aud_get_service_handle((void *)g_voice_ctx.voice_handle, AUD_SERVICE_AI_VOC);
#endif

    // 初始化录音
    voice_read_cfg_t voice_read_cfg = VOICE_READ_CFG_DEFAULT();
    voice_read_cfg.voice_handle = g_voice_ctx.voice_handle;
    voice_read_cfg.voice_read_callback = config->mic_rx_cb;
    voice_read_cfg.args = config->user_data;

    g_voice_ctx.voice_read_handle = bk_voice_read_init(&voice_read_cfg);
    if (!g_voice_ctx.voice_read_handle) {
        LOGE("voice read init failed\n");
        goto error;
    }

    // 初始化播放
    voice_write_cfg_t voice_write_cfg = VOICE_WRITE_CFG_DEFAULT();
    voice_write_cfg.voice_handle = g_voice_ctx.voice_handle;
    voice_write_cfg.node_num = SPK_FRAME_NUM;
    if (config->dec_type == AUDIO_DEC_TYPE_OPUS) {
        voice_write_cfg.write_buf_type = PORT_TYPE_FB;
        voice_write_cfg.node_size = OPUS_DEC_FRAME_SIZE;
    } else {
        voice_write_cfg.write_buf_type = PORT_TYPE_RB;
        voice_write_cfg.node_size = config->spk_sample_rate * 2 * config->spk_frame_duration / 1000;
    }
    
    g_voice_ctx.voice_write_handle = bk_voice_write_init(&voice_write_cfg);
    if (!g_voice_ctx.voice_write_handle) {
        LOGE("voice write init failed\n");
        goto error;
    }

    // 启动语音服务
    if (BK_OK != bk_voice_start(g_voice_ctx.voice_handle)) {
        LOGE("voice start failed\n");
        goto error;
    }

    // 启动录音
    if (BK_OK != bk_voice_read_start(g_voice_ctx.voice_read_handle)) {
        LOGE("voice read start failed\n");
        goto error;
    }

    // 启动播放
    if (BK_OK != bk_voice_write_start(g_voice_ctx.voice_write_handle)) {
        LOGE("voice write start failed\n");
        goto error;
    }

#if CONFIG_ASR_SERVICE
    // 启动ASR
    if (BK_OK != bk_asr_start(g_voice_ctx.asr_handle)) {
        LOGE("asr start failed\n");
        goto error;
    }
    if (BK_OK != bk_aud_asr_start(g_voice_ctx.aud_asr_handle)) {
        LOGE("aud asr start failed\n");
        goto error;
    }
#endif

#if CONFIG_PLAYER_SERVICE
    if (config->prompt_tone_enable) {
        audio_prompt_tone_cfg_t prompt_tone_cfg = DEFAULT_AUDIO_PROMPT_TONE_CONFIG();
        prompt_tone_cfg.port_id = AUDIO_SPK_PORT_TONE;
        prompt_tone_cfg.dec_type = config->prompt_tone_dec_type;
        prompt_tone_cfg.spk_stream = bk_voice_get_spk_element(g_voice_ctx.voice_handle);
        g_voice_ctx.prompt_tone_enable = true;
        g_voice_ctx.prompt_tone_handle = audio_prompt_tone_init(&prompt_tone_cfg);
    }
#endif

#if CONFIG_AUDIO_PLAYER
    if (config->audio_player_enable) {
        ringbuf_port_cfg_t rb_cfg = RINGBUF_PORT_CFG_DEFAULT();
        rb_cfg.ringbuf_size = 10 * 1024;
        g_voice_ctx.audio_player_port_handle = ringbuf_port_init(&rb_cfg);
        if (!g_voice_ctx.audio_player_port_handle) {
            LOGE("ringbuf_port_init failed\r\n");
            goto error;
        }

        app_audio_player_cfg_t player_cfg = DEFAULT_APP_AUDIO_PLAYER_CONFIG();
        player_cfg.port_id = AUDIO_SPK_PORT_PLAYER;
        player_cfg.priority = AUDIO_SPK_PORT_PRIORITY_PLAYER;
        player_cfg.rb_port = g_voice_ctx.audio_player_port_handle;
        player_cfg.spk_stream = bk_voice_get_spk_element(g_voice_ctx.voice_handle);
        player_cfg.event_handler = config->audio_player_event_cb;
        player_cfg.user_data = config->user_data;

        if (BK_OK != app_audio_player_init(&player_cfg)) {
            LOGE("audio player init failed\r\n");
            goto error;
        }

        g_voice_ctx.audio_player_enable = true;
    }
#endif

    g_voice_ctx.inited = true;
    LOGI("app voice init success\n");
    
    return BK_OK;

error:
#if CONFIG_PLAYER_SERVICE
    if (g_voice_ctx.prompt_tone_handle) {
        audio_prompt_tone_deinit(g_voice_ctx.prompt_tone_handle);
        g_voice_ctx.prompt_tone_handle = NULL;
    }
#endif

#if CONFIG_AUDIO_PLAYER
    if (g_voice_ctx.audio_player_enable) {
        app_audio_player_deinit();
    }

    if (g_voice_ctx.audio_player_port_handle) {
        audio_port_deinit(g_voice_ctx.audio_player_port_handle);
        g_voice_ctx.audio_player_port_handle = NULL;
    }
#endif

    if (g_voice_ctx.voice_write_handle) {
        bk_voice_write_stop(g_voice_ctx.voice_write_handle);
    }
    
    if (g_voice_ctx.voice_read_handle) {
        bk_voice_read_stop(g_voice_ctx.voice_read_handle);
    }

#if CONFIG_ASR_SERVICE
    if (g_voice_ctx.asr_handle) {
        bk_asr_stop(g_voice_ctx.asr_handle);
    }
    
    if (g_voice_ctx.aud_asr_handle) {
        bk_aud_asr_stop(g_voice_ctx.aud_asr_handle);
    }
#endif
    
    if (g_voice_ctx.voice_handle) {
        bk_voice_stop(g_voice_ctx.voice_handle);
    }
    
    if (g_voice_ctx.voice_write_handle) {
        bk_voice_write_deinit(g_voice_ctx.voice_write_handle);
        g_voice_ctx.voice_write_handle = NULL;
    }
    
    if (g_voice_ctx.voice_read_handle) {
        bk_voice_read_deinit(g_voice_ctx.voice_read_handle);
        g_voice_ctx.voice_read_handle = NULL;
    }
    
#if CONFIG_ASR_SERVICE
    if (g_voice_ctx.aud_asr_handle) {
        bk_aud_asr_deinit(g_voice_ctx.aud_asr_handle);
        g_voice_ctx.aud_asr_handle = NULL;
    }
    
    if (g_voice_ctx.asr_handle) {
        bk_asr_deinit(g_voice_ctx.asr_handle);
        g_voice_ctx.asr_handle = NULL;
    }
#endif
    
    if (g_voice_ctx.voice_handle) {
        bk_voice_deinit(g_voice_ctx.voice_handle);
        g_voice_ctx.voice_handle = NULL;
    }
    
    return BK_FAIL;
}

bk_err_t bk_app_audio_deinit(void)
{
    if (!g_voice_ctx.inited) {
        LOGW("app voice service not initialized\n");
        return BK_OK;
    }

#if CONFIG_PLAYER_SERVICE
    if (g_voice_ctx.prompt_tone_handle) {
        audio_prompt_tone_deinit(g_voice_ctx.prompt_tone_handle);
        g_voice_ctx.prompt_tone_handle = NULL;
    }
#endif

#if CONFIG_AUDIO_PLAYER
    if (g_voice_ctx.audio_player_enable) {
        app_audio_player_deinit();
    }

    if (g_voice_ctx.audio_player_port_handle) {
        audio_port_deinit(g_voice_ctx.audio_player_port_handle);
        g_voice_ctx.audio_player_port_handle = NULL;
    }
#endif

    if (g_voice_ctx.voice_read_handle) {
        bk_voice_read_stop(g_voice_ctx.voice_read_handle);
    }

    if (g_voice_ctx.voice_write_handle) {
        bk_voice_write_stop(g_voice_ctx.voice_write_handle);
    }

#if CONFIG_ASR_SERVICE
    if (g_voice_ctx.asr_handle) {
        bk_asr_stop(g_voice_ctx.asr_handle);
    }

    if (g_voice_ctx.aud_asr_handle) {
        bk_aud_asr_stop(g_voice_ctx.aud_asr_handle);
    }
#endif

    if (g_voice_ctx.voice_handle) {
        bk_voice_stop(g_voice_ctx.voice_handle);
    }

    if (g_voice_ctx.voice_read_handle) {
        bk_voice_read_deinit(g_voice_ctx.voice_read_handle);
        g_voice_ctx.voice_read_handle = NULL;
    }

    if (g_voice_ctx.voice_write_handle) {
        bk_voice_write_deinit(g_voice_ctx.voice_write_handle);
        g_voice_ctx.voice_write_handle = NULL;
    }

#if CONFIG_ASR_SERVICE
    if (g_voice_ctx.aud_asr_handle) {
        bk_aud_asr_deinit(g_voice_ctx.aud_asr_handle);
        g_voice_ctx.aud_asr_handle = NULL;
    }

    if (g_voice_ctx.asr_handle) {
        bk_asr_deinit(g_voice_ctx.asr_handle);
        g_voice_ctx.asr_handle = NULL;
    }
#endif

    if (g_voice_ctx.voice_handle) {
        bk_voice_deinit(g_voice_ctx.voice_handle);
        g_voice_ctx.voice_handle = NULL;
    }

    g_voice_ctx.inited = false;
    os_memset(&g_voice_ctx, 0, sizeof(app_voice_ctx_t));

    LOGI("app voice deinit success\n");
    
    return BK_OK;
}

bk_err_t bk_app_audio_ensure_voice_streaming(void)
{
    return bk_app_audio_recover_voice_streaming(false);
}

bk_err_t bk_app_audio_recover_voice_streaming(bool force_restart)
{
    bk_err_t ret = BK_OK;
    voice_sta_t status = VOICE_STA_NONE;

    if (!g_voice_ctx.inited || !g_voice_ctx.voice_handle || !g_voice_ctx.voice_read_handle || !g_voice_ctx.voice_write_handle) {
        LOGE("app voice service not initialized\n");
        return BK_FAIL;
    }

    ret = bk_voice_get_status(g_voice_ctx.voice_handle, &status);
    if (ret != BK_OK) {
        LOGE("get voice status failed, ret: %d\n", ret);
        return ret;
    }

    if (force_restart && status == VOICE_STA_RUNNING) {
        LOGW("force voice stream restart, status:%d\n", status);

        ret = bk_voice_read_stop(g_voice_ctx.voice_read_handle);
        if (ret != BK_OK) {
            LOGW("voice read stop before restart failed, ret:%d\n", ret);
        }

        ret = bk_voice_write_stop(g_voice_ctx.voice_write_handle);
        if (ret != BK_OK) {
            LOGW("voice write stop before restart failed, ret:%d\n", ret);
        }

        ret = bk_voice_stop(g_voice_ctx.voice_handle);
        if (ret != BK_OK) {
            LOGE("voice stop before restart failed, ret:%d\n", ret);
            return ret;
        }

        status = VOICE_STA_STOPED;
        rtos_delay_milliseconds(20);
    }

    if (status != VOICE_STA_RUNNING) {
        LOGW("voice stream not running, status:%d, restart\n", status);
        ret = bk_voice_start(g_voice_ctx.voice_handle);
        if (ret != BK_OK) {
            LOGE("voice restart failed, status:%d, ret:%d\n", status, ret);
            return ret;
        }
    }

    ret = bk_voice_read_start(g_voice_ctx.voice_read_handle);
    if (ret != BK_OK) {
        LOGE("voice read restart failed, ret:%d\n", ret);
        return ret;
    }

    ret = bk_voice_write_start(g_voice_ctx.voice_write_handle);
    if (ret != BK_OK) {
        LOGE("voice write restart failed, ret:%d\n", ret);
        return ret;
    }

    LOGI("voice stream recovered, status:%d, force:%d\n", status, force_restart);
    return BK_OK;
}

bk_err_t bk_app_audio_stop_prompt_tone(void)
{
#if CONFIG_PLAYER_SERVICE
    bk_err_t ret = BK_OK;

    if (!g_voice_ctx.inited) {
        LOGW("app voice service not initialized\n");
        return BK_OK;
    }

    if (!g_voice_ctx.prompt_tone_handle) {
        LOGW("prompt tone not started\r\n");
        return BK_OK;
    }

    ret = audio_prompt_tone_stop(g_voice_ctx.prompt_tone_handle);
    if (ret != BK_OK) {
        LOGW("audio prompt tone stop failed, ret:%d\n", ret);
        return ret;
    }

    LOGI("prompt tone stopped\n");
    return BK_OK;
#else
    LOGW("CONFIG_PLAYER_SERVICE is not set\r\n");
    return BK_OK;
#endif
}

bk_err_t bk_app_audio_write_data(uint8_t *data, uint32_t len)
{
    if (!g_voice_ctx.inited) {
        LOGE("app voice service not initialized\n");
        return BK_FAIL;
    }

    if (!data || !len) {
        LOGE("invalid data or len\n");
        return BK_ERR_PARAM;
    }

    return bk_voice_write_frame_data(g_voice_ctx.voice_write_handle, (char *)data, len);
}

bk_err_t bk_app_audio_set_volume(uint8_t volume)
{
    bk_err_t ret = BK_OK;

    if (!g_voice_ctx.inited) {
        LOGE("app voice service not initialized\n");
        return BK_FAIL;
    }

    if (volume > 100) {
        LOGE("volume %d is out of range (0-100)\n", volume);
        return BK_ERR_PARAM;
    }

    // 将0-100映射到 [0, SPK_GAIN_MAX]
    uint8_t gain = (uint8_t)(volume * SPK_GAIN_MAX / 100);
    LOGI("speaker volume to gain, volume: %d, gain: 0x%02x\n", volume, gain);
    
    audio_element_handle_t spk_element = bk_voice_get_spk_element(g_voice_ctx.voice_handle);
    if (!spk_element) {
        LOGE("get speaker element failed\n");
        return BK_FAIL;
    }

    ret = onboard_speaker_stream_set_digital_gain(spk_element, gain);
    if (ret != BK_OK) {
        LOGE("set speaker digital gain failed: %d\n", ret);
        return ret;
    }

    return BK_OK;
}

bk_err_t bk_app_audio_pa_control(bool enable)
{
    if (!g_voice_ctx.inited) {
        LOGE("app voice service not initialized\n");
        return BK_FAIL;
    }

    if (g_voice_ctx.pa_ctrl_gpio == 0xFFFF) {
        LOGE("PA control GPIO not configured\n");
        return BK_FAIL;
    }

    if (g_voice_ctx.pa_ctrl_gpio >= GPIO_NUM) {
        LOGE("PA control GPIO %d is invalid, max GPIO is %d\n", g_voice_ctx.pa_ctrl_gpio, GPIO_NUM - 1);
        return BK_FAIL;
    }

    if (enable) {
        if (g_voice_ctx.pa_on_level) {
            bk_gpio_set_output_high(g_voice_ctx.pa_ctrl_gpio);
        } else {
            bk_gpio_set_output_low(g_voice_ctx.pa_ctrl_gpio);
        }
        LOGI("PA turned on\n");
    } else {
        if (g_voice_ctx.pa_on_level) {
            bk_gpio_set_output_low(g_voice_ctx.pa_ctrl_gpio);
        } else {
            bk_gpio_set_output_high(g_voice_ctx.pa_ctrl_gpio);
        }
        LOGI("PA turned off\n");
    }

    return BK_OK;
}

bk_err_t bk_app_audio_play_prompt_tone(player_uri_type_t uri_type, char *data, uint32_t len)
{
#if CONFIG_PLAYER_SERVICE
    bk_err_t ret = BK_OK;

    if (!g_voice_ctx.inited) {
        LOGE("app voice service not started\r\n");
        return BK_FAIL;
    }

    if (!g_voice_ctx.prompt_tone_handle) {
        LOGE("prompt tone not started\r\n");
        return BK_FAIL;
    }

    audio_prompt_tone_info_t prompt_tone_info = {
        .uri_type = uri_type,
        .data     = data,
        .len      = len,
    };

    ret = audio_prompt_tone_start(g_voice_ctx.prompt_tone_handle, &prompt_tone_info);
    if (ret != BK_OK) {
        LOGE("audio prompt tone start fail, ret: %d\n", ret);
        return ret;
    }

    return BK_OK;
#else
    LOGE("CONFIG_PLAYER_SERVICE is not set\r\n");
    return BK_FAIL;
#endif
}

audio_element_handle_t bk_app_audio_get_spk_element(void)
{
    if (!g_voice_ctx.inited || !g_voice_ctx.voice_handle) {
        LOGE("app voice service not initialized\n");
        return NULL;
    }

    return bk_voice_get_spk_element(g_voice_ctx.voice_handle);
}
