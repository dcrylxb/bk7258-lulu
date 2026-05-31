#include "components/log.h"

#include "env_module.h"
#include "dialog_module.h"
#include "system_manager.h"
// #include "vad_module.h"
#include "protocol.h"
#include "bk_app_audio.h"

#define TAG "dialog"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define DIALOG_PROMPT_KWS_SUPPRESS_MS 1800
#define DIALOG_PROMPT_WAKE_KWS_SUPPRESS_MS 30000
#define DIALOG_PROMPT_BYE_SUPPRESS_MS 20000

static uint32_t s_prompt_kws_suppress_until_ms = 0;
static uint32_t s_prompt_bye_suppress_until_ms = 0;

static bool _dialog_time_before(uint32_t deadline_ms, uint32_t now_ms)
{
    return (int32_t)(deadline_ms - now_ms) > 0;
}

static bool _dialog_prompt_needs_extended_kws_suppress(const char *path)
{
    if (path == NULL) {
        return false;
    }

    return (os_strstr(path, "test_wake_prompt") != NULL)
        || (os_strstr(path, "wake_confirm") != NULL);
}

static void _dialog_note_prompt_playback_started(const char *path)
{
    uint32_t now_ms = rtos_get_time();
    uint32_t kws_suppress_ms = DIALOG_PROMPT_KWS_SUPPRESS_MS;

    if (_dialog_prompt_needs_extended_kws_suppress(path)) {
        kws_suppress_ms = DIALOG_PROMPT_WAKE_KWS_SUPPRESS_MS;
    }

    s_prompt_kws_suppress_until_ms = now_ms + kws_suppress_ms;
    s_prompt_bye_suppress_until_ms = now_ms + DIALOG_PROMPT_BYE_SUPPRESS_MS;
    LOGI("prompt kws suppress until=%u bye_until=%u now=%u path=%s\r\n",
         s_prompt_kws_suppress_until_ms,
         s_prompt_bye_suppress_until_ms,
         now_ms,
         path);
}

static bool _dialog_prompt_kws_suppress_active(void)
{
    uint32_t now_ms = rtos_get_time();

    if (s_prompt_kws_suppress_until_ms == 0) {
        return false;
    }

    if (_dialog_time_before(s_prompt_kws_suppress_until_ms, now_ms)) {
        return true;
    }

    s_prompt_kws_suppress_until_ms = 0;
    return false;
}

static bool _dialog_prompt_bye_suppress_active(void)
{
    uint32_t now_ms = rtos_get_time();

    if (s_prompt_bye_suppress_until_ms == 0) {
        return false;
    }

    if (_dialog_time_before(s_prompt_bye_suppress_until_ms, now_ms)) {
        return true;
    }

    s_prompt_bye_suppress_until_ms = 0;
    return false;
}

#if CONFIG_DUMP_AUDIO_TO_VFS
#include "components/bk_audio/audio_utils/vfs_util.h"

static struct vfs_util g_mic_dump_handle = {0};
#define MIC_DUMP_PCM_FILE     "/sf0/dialog_mic_dump.pcm"
#define MIC_DUMP_OPUS_FILE    "/sf0/dialog_mic_dump.opus"

static void _dialog_data_dump_open(void)
{
    bk_err_t ret = BK_OK;
    dialog_module_t *module = dialog_module_instance();
    char *file_name = NULL;

    if (module->m_mic_enc_type == AUDIO_ENC_TYPE_OPUS) {
        file_name = MIC_DUMP_OPUS_FILE;
    } else {
        file_name = MIC_DUMP_PCM_FILE;
    }

    LOGI("dump file name: %s\r\n", file_name);

    ret = vfs_util_create(&g_mic_dump_handle, file_name);
    if (BK_OK != ret) {
        LOGE("create dump file fail, ret: %d\r\n", ret);
        return;
    }
}

static void _dialog_data_dump_close(void)
{
    vfs_util_destroy(&g_mic_dump_handle);
    memset(&g_mic_dump_handle, 0, sizeof(g_mic_dump_handle));
}

static void _dialog_data_dump_mic_data(void *data, uint32_t size)
{
    uint16_t frame_len = size;
    dialog_module_t *module = dialog_module_instance();
    
    if (module->m_dump_is_start) {
        if (module->m_mic_enc_type == AUDIO_ENC_TYPE_OPUS) {
            // 2Byte len + NByte data
            vfs_util_tx_data(&g_mic_dump_handle, (uint8_t *)&frame_len, sizeof(frame_len));
            vfs_util_tx_data(&g_mic_dump_handle, data, size);
        } else {
            vfs_util_tx_data(&g_mic_dump_handle, data, size);
        }
    }
}

#define DIALOG_DATA_DUMP_OPEN()                        _dialog_data_dump_open()
#define DIALOG_DATA_DUMP_CLOSE()                       _dialog_data_dump_close()
#define DIALOG_DATA_DUMP_MIC_DATA(data, len)           _dialog_data_dump_mic_data(data, len)

#else

#define DIALOG_DATA_DUMP_OPEN()
#define DIALOG_DATA_DUMP_CLOSE()
#define DIALOG_DATA_DUMP_MIC_DATA(data, len)

#endif  //CONFIG_DUMP_AUDIO_TO_VFS

static void _dump_data_start(void)
{
    dialog_module_t *module = dialog_module_instance();

    if (module->m_dump_is_start) {
        LOGE("dump already started\r\n");
        return;
    }
    
    DIALOG_DATA_DUMP_OPEN();

    module->m_dump_is_start = true;
    
    LOGI("dump start success\r\n");

    return;
}

static void _dump_data_stop(void)
{
    dialog_module_t *module = dialog_module_instance();
    
    if (!module->m_dump_is_start) {
        LOGE("dump not started\r\n");
        return;
    }
   
    module->m_dump_is_start = false;

    DIALOG_DATA_DUMP_CLOSE();

    LOGI("dump stop success\r\n");

    return;
}

static int _mic_data_callback(unsigned char *data, unsigned int size, void *args)
{
    dialog_module_t *module = dialog_module_instance();
    system_manager_module_t *system_module = system_manager_instance();
    system_status_e system_status = system_module->get_system_status();
    bool forwarded = false;

    DIALOG_DATA_DUMP_MIC_DATA(data, size);

    module->m_voice_diag.mic_cb_total++;
    module->m_voice_diag.mic_cb_bytes += size;
    module->m_voice_diag.mic_last_status = (int)system_status;
    module->m_voice_diag.mic_last_len = size;
    module->m_voice_diag.mic_last_cb_ms = rtos_get_time();

#if CONFIG_PROTOCOL_USE_WSS
    if (SYSTEM_STATUS_RECORDING == system_status) //录音态
#elif CONFIG_PROTOCOL_USE_MQTT
    //LOGD("[%s][%d]system_status:%d\r\n", __FUNCTION__, __LINE__, system_status);
    if (SYSTEM_STATUS_SERV_CONNECT_OK <= system_status) //配网完成态
#endif
    {
        module->m_voice_diag.mic_cb_recording++;
        protocol_instance()->sendAudio((uint8_t*)data, size);
        module->m_voice_diag.mic_cb_forwarded++;
        module->m_voice_diag.mic_forwarded_bytes += size;
        forwarded = true;
    } else {
        module->m_voice_diag.mic_cb_dropped_status++;
    }

    if ((module->m_voice_diag.mic_cb_total % 200) == 1) {
        LOGI("voice diag cb=%u rec=%u fwd=%u drop_status=%u bytes=%u fwd_bytes=%u last_status=%d last_len=%u last_ms=%u\r\n",
             module->m_voice_diag.mic_cb_total,
             module->m_voice_diag.mic_cb_recording,
             module->m_voice_diag.mic_cb_forwarded,
             module->m_voice_diag.mic_cb_dropped_status,
             module->m_voice_diag.mic_cb_bytes,
             module->m_voice_diag.mic_forwarded_bytes,
             module->m_voice_diag.mic_last_status,
             module->m_voice_diag.mic_last_len,
             module->m_voice_diag.mic_last_cb_ms);
    }

    if (forwarded) {
        return (int)size;
    }

    return 0;
}

static void _asr_result_callback(uint32_t param)
{
    char *result = (char *)param;
    
    LOGI("asr result: %s\n", result);

    if (_dialog_prompt_kws_suppress_active()) {
        LOGI("offline kws suppressed during prompt playback: %s\n", result);
        return;
    }

    if (os_strcmp(result, DEV_WAKE_WORD) == 0) {
        LOGI("offline kws matched wake word: %s\n", result);
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_DIALOG_START);
    } else if (os_strcmp(result, DEV_BYE_WORD) == 0) {
        if (_dialog_prompt_bye_suppress_active()) {
            LOGI("offline bye word suppressed after prompt playback: %s\n", result);
            return;
        }
#if DEV_BYE_WORD_ENABLED
        LOGI("offline kws matched bye word: %s\n", result);
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_LAST_SENTENCE);
#else
        LOGI("offline kws matched disabled bye word: %s\n", result);
#endif
    } else {
        LOGI("offline kws ignored result: %s\n", result);
    }
}

#if CONFIG_VAD
static int _vad_state_update_cb(int32_t vad_state)
{
    LOGI("vad state: %d\n", vad_state);

    if (vad_state == VAD_SPEECH_START) {
        LOGE("start record\r\n");
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_RECORD_START);
    } else if (vad_state == VAD_SPEECH_END) {
        LOGE("stop record\r\n");
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_RECORD_END);
    }

    return 0;
}
#endif

#if CONFIG_AUDIO_PLAYER
static void _audio_player_event_handler(audio_player_event_type_t event, void *extra_info, void *args)
{
    (void)extra_info;
    (void)args;

    switch (event) {
        case AUDIO_PLAYER_EVENT_SONG_FINISH:
            CLI_LOGI("Song finished\n");
            break;

        case AUDIO_PLAYER_EVENT_SONG_START:
            CLI_LOGI("Song started\n");
            break;

        case AUDIO_PLAYER_EVENT_SONG_PAUSE:
            CLI_LOGI("Song paused\n");
            break;

        case AUDIO_PLAYER_EVENT_SONG_RESUME:
            CLI_LOGI("Song resumed\n");
            break;

        case AUDIO_PLAYER_EVENT_SONG_FAILURE:
            CLI_LOGI("Song playback failed\n");
            break;
 
        case AUDIO_PLAYER_EVENT_SONG_TICK:
            if (extra_info != NULL)
            {
                int second = *((int *)extra_info);
                CLI_LOGI("Song tick, %d\n", second);
            }
            break;

        case AUDIO_PLAYER_EVENT_SEEK_COMPLETE:
            CLI_LOGI("Seek complete event received\n");
            break;

        default:
            break;
    }
}
#endif

static s32 _dialog_init(void)
{
    dialog_module_t *module = dialog_module_instance();
    bk_err_t ret = BK_FAIL;

    if (module->m_is_init) {
        return BK_OK;
    }

    app_audio_config_t cfg = DEFAULT_APP_AUDIO_CONFIG();
    cfg.aec_mode = AEC_MODE_SOFTWARE;
    cfg.pa_ctrl_gpio = SPEAKER_PA_GPIO;
    cfg.pa_on_level = 1;
#if CONFIG_AUDIO_PLAYER
    cfg.audio_player_enable = true;
    cfg.audio_player_event_cb = _audio_player_event_handler;
#endif
#if CONFIG_PLAYER_SERVICE
    cfg.prompt_tone_enable = true;
#endif
    cfg.dec_type = AUDIO_DEC_TYPE_OPUS;
    cfg.spk_sample_rate = SPK_SAMPLE_RATE;
    cfg.spk_frame_duration = SPK_FRAME_DURATION;
    cfg.enc_type = AUDIO_ENC_TYPE_OPUS;
    cfg.mic_sample_rate = MIC_SAMPLE_RATE;
    cfg.mic_rx_cb = _mic_data_callback;
#if CONFIG_WANSON_ARMINO_ASR
    cfg.asr_frame_size = 960; // Wanson_ASR_Recog 一帧 30ms，16000*30/1000*2=960
#elif CONFIG_BEKEN_KWS
    cfg.asr_frame_size = 1280; // Beken KWS asr 一帧 40ms，16000*40/1000*2=1280
#endif
    cfg.asr_result_cb = _asr_result_callback;
#if CONFIG_VAD
#if CONFIG_PROTOCOL_USE_WSS
    /*
     * WSS realtime mode needs continuous Opus frames after listen.start.
     * The AEC VAD path gates its output after VAD_SPEECH_END, which can starve
     * the voice read task while the server is still waiting for realtime audio.
     */
    cfg.vad_enable = false;
    LOGI("WSS realtime audio VAD disabled for continuous uplink\r\n");
#else
    cfg.vad_enable = true;
#endif
    cfg.vad_state_cb = _vad_state_update_cb;
#endif
    ret = bk_app_audio_init(&cfg);
    if (BK_OK != ret) {
        LOGE("app voice init fail, ret: %d\r\n", ret);
        return ret;
    }

    if (env_module_instance()->getSpkInfo(&module->m_spk_volume) != 0 ) {
        LOGI("get spk volume fail, set default volume %d\r\n", DEFAULT_SPK_VOL);
        module->m_spk_volume = DEFAULT_SPK_VOL;
        env_module_instance()->setSpkInfo(&module->m_spk_volume);
    }

    if (module->m_spk_volume < DEFAULT_SPK_VOL) {
        LOGI("raise stored speaker volume from %d to %d\r\n", module->m_spk_volume, DEFAULT_SPK_VOL);
        module->m_spk_volume = DEFAULT_SPK_VOL;
        env_module_instance()->setSpkInfo(&module->m_spk_volume);
    }

    ret = bk_app_audio_set_volume(module->m_spk_volume);
    if (BK_OK != ret) {
        LOGE("set spk volume fail, ret: %d\r\n", ret);
    }
    
    module->m_mic_enc_type = cfg.enc_type;
    module->m_is_init = true;
    LOGI("init success\r\n");
    return BK_OK;
}

static s32 _dialog_start(void)
{
    dialog_module_t *module = dialog_module_instance();

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return BK_FAIL;
    }

    module->speaker_pa_control(true);
    LOGI("start success\r\n");
    return BK_OK;
}

static s32 _dialog_stop(void)
{
    dialog_module_t *module = dialog_module_instance();

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return BK_FAIL;
    }

    module->speaker_pa_control(false);
    LOGI("stop success\r\n");
    return BK_OK;
}

static s32 _dialog_deinit(void)
{
    dialog_module_t *module = dialog_module_instance();
    bk_err_t ret = BK_FAIL;

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return ret;
    }

    _dump_data_stop();

    ret = bk_app_audio_deinit();
    if (BK_OK != ret) {
        LOGE("app voice deinit fail, ret: %d\r\n", ret);
        return ret;
    }

    module->m_is_init = false;
    LOGI("deinit success\r\n");
    return BK_OK;
}

static s32 _dialog_write_speaker_data(u8 *speaker_data, uint32 speaker_data_len)
{
    dialog_module_t *module = dialog_module_instance();
    s32 ret = 0;

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return BK_FAIL;
    }

    LOGI("play data fill %d byte\r\n", speaker_data_len);

    ret = bk_app_audio_write_data(speaker_data, speaker_data_len);
    if (ret < 0) {
        LOGE("speaker write data fail, ret: %d\r\n", ret);
    }

    return ret;
}

static void _dialog_speaker_control(bool onoff)
{
    static int val = -1;

    if (val != onoff) {
        val = onoff;
        if (onoff) {
            LOGI("PA on\r\n");
            bk_app_audio_pa_control(true);
        } else {
            LOGI("PA off\r\n");
            bk_app_audio_pa_control(false);
        }
    }
}

static s32 _dialog_speaker_play_abort(void)
{
    dialog_module_t *module = dialog_module_instance();
    s32 ret = BK_FAIL;

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return ret;
    }

    _dialog_speaker_control(false);   //关闭功放
    LOGI("speaker play abort success\r\n");
    return BK_OK;
}

static void _dialog_set_spk_volume(u8 volume)
{
    if (BK_OK == bk_app_audio_set_volume(volume)) {
        LOGI("speaker volume: %d\r\n", volume);
        dialog_module_instance()->m_spk_volume = volume;
        env_module_instance()->setSpkInfo(&(dialog_module_instance()->m_spk_volume));
    } else {
        LOGE("set spk volume fail\r\n");
    }
}

static void _dialog_speaker_play_prompt_tone(char *path)
{
    dialog_module_t *module = dialog_module_instance();
    bk_err_t ret = BK_FAIL;

    if (!module->m_is_init) {
        LOGE("module don't init\r\n");
        return;
    }

    if (path == NULL) {
        LOGE("path is NULL\r\n");
        return;
    }

    _dialog_note_prompt_playback_started(path);
    ret = bk_app_audio_play_prompt_tone(PLAYER_URI_TYPE_VFS, path, strlen(path));
    if (ret < 0) {
        LOGE("speaker play prompt tone fail, ret: %d\r\n", ret);
    }

    return;
}

static dialog_module_t g_dialog_module =
{
    .m_is_init = false,
    
    .super.init = _dialog_init,
    .super.start = _dialog_start,
    .super.stop = _dialog_stop,
    .super.deinit = _dialog_deinit,

    .write_speaker_data = _dialog_write_speaker_data,
    .speaker_pa_control = _dialog_speaker_control,
    .speaker_play_abort = _dialog_speaker_play_abort,
    .speaker_set_volume = _dialog_set_spk_volume,
    .speaker_play_prompt_tone = _dialog_speaker_play_prompt_tone,

    .dump_data_start    = _dump_data_start,
    .dump_data_stop     = _dump_data_stop,
};

dialog_module_t *dialog_module_instance(void)
{
    return &g_dialog_module;
}

void dialog_voice_diag_get(dialog_voice_diag_t *diag)
{
    if (diag == NULL) {
        return;
    }

    *diag = dialog_module_instance()->m_voice_diag;
}

void dialog_voice_diag_note_ws_audio_tx(int len, int ret)
{
    dialog_voice_diag_t *diag = &dialog_module_instance()->m_voice_diag;

    diag->ws_audio_last_len = len;
    diag->ws_audio_last_ret = ret;
    diag->ws_audio_tx_count++;

    if (ret >= 0) {
        diag->ws_audio_tx_bytes += len;
    } else {
        diag->ws_audio_tx_fail++;
    }
}
