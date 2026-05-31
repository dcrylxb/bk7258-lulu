#include "os/os.h"
#include "os/str.h"
#include "os/mem.h"
#include "components/log.h"

#include "system_manager.h"
#include "boards_common.h"
#include "net_config.h"
#include "env_module.h"
#include "ota_module.h"
#include "dialog_module.h"
#include "protocol.h"
#include "pet_brain.h"
#include "pet_behavior_runtime.h"
#include "pet_scene.h"
#include "bk_app_audio.h"

#if (CONFIG_LCD)
#include "display_module.h"
#endif

#if (CONFIG_AUDIO_PLAYER)
#include "app_audio_player.h"
#endif

#define TAG "sys_man"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define VOICE_MIC_CB_STALE_MS 6000

static bool _voice_status_can_start_listen(system_status_e status)
{
    return (status == SYSTEM_STATUS_SERV_CONNECT_OK || status == SYSTEM_STATUS_PLAYING);
}

static bool _voice_status_can_auto_listen_after_play(system_status_e status)
{
    return status == SYSTEM_STATUS_PLAYING;
}

static bk_err_t _recover_voice_stream_before_listen(const char *reason)
{
    dialog_voice_diag_t diag = {0};
    uint32_t now_ms = rtos_get_time();
    uint32_t cb_age_ms = 0;
    bool force_restart = false;

    dialog_voice_diag_get(&diag);
    if (diag.mic_last_cb_ms > 0) {
        cb_age_ms = now_ms - diag.mic_last_cb_ms;
        force_restart = (cb_age_ms > VOICE_MIC_CB_STALE_MS);
    } else if (now_ms > VOICE_MIC_CB_STALE_MS) {
        cb_age_ms = now_ms;
        force_restart = true;
    }

    if (force_restart) {
        LOGW("voice mic callback stale before %s listen, age:%u last:%u now:%u\r\n",
             reason ? reason : "unknown",
             cb_age_ms,
             diag.mic_last_cb_ms,
             now_ms);
    }

    return bk_app_audio_recover_voice_streaming(force_restart);
}

static void _manager_task(void *arg)
{
    system_manager_module_t *module = system_manager_instance();
    bk_err_t ret = BK_FAIL;
    sysMsg_t msg = {0};
    int last_status = 0;
    pet_brain_snapshot_t snapshot = {0};

    module->m_task_state = TASK_STATE_RUNNING;
    rtos_set_semaphore(&module->m_sem);

    while(TASK_STATE_RUNNING == module->m_task_state)
    {   
        ret = rtos_pop_from_queue(&module->m_queue, &msg, BEKEN_WAIT_FOREVER);
        if(ret != kNoErr)
        {
            LOGE("get queue fail\r\n");
            continue;
        }

        switch(msg.event)
        {
            case SYSTEM_EVENT_INIT:

                //ota模块init
                ota_instance()->super.init();

                //对话模块init
                dialog_module_instance()->super.init();
                dialog_module_instance()->super.start();

                //环境变量模块init
                env_module_instance()->super.init();

                //网络模块init
                net_config_instance()->super.init();
                net_config_instance()->super.start();

#if (CONFIG_LCD)
                display_module_instance()->super.init();
#endif

                pet_behavior_runtime_init();
                pet_brain_init();
                pet_scene_init();

                module->m_system_status = SYSTEM_STATUS_INIT;
                LOGI("init ok\r\n");
                break;

            case SYSTEM_EVENT_NET_NULL:                       //等待配网
                LOGI("wait cfg wifi\r\n");
                module->m_system_status = SYSTEM_STATUS_NET_NULL;
                dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION);
                break;

            case SYSTEM_EVENT_NET_CONFIGING:                  //配网中
                {
                    LOGI("configing\r\n");
                    net_info_t *net_info = (net_info_t*)msg.param;
                    
                    net_config_instance()->connect_ap(net_info->ssid, net_info->pwd, true);
                    
                    msg.param = 0;
                    
                    module->m_system_status = SYSTEM_STATUS_NET_CONFIGING;
                }
                break;

            case SYSTEM_EVENT_NET_CONFIG_DONE:                  //配网完成
                {
                    LOGI("net config success\r\n");
                    wifi_sta_config_t sta_config = {0};
                    net_info_t net_info = {0};

                    bk_wifi_sta_get_config(&sta_config);
                    os_snprintf(net_info.ssid, WIFI_SSID_STR_LEN, "%s", sta_config.ssid);
                    os_snprintf(net_info.pwd, WIFI_PASSWORD_LEN, "%s", sta_config.password);
                    env_module_instance()->setNetInfo(&net_info);
                    module->m_system_status = SYSTEM_STATUS_NET_CONFIG_DONE;
                    dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION_SUCCESS);

                    if(!ota_instance()->isActive())
                    {
                        ota_instance()->super.start();
                    }
                }
                break;

            case SYSTEM_EVENT_NET_CONNECTING:
                LOGI("net connecting\r\n");
                module->m_system_status = SYSTEM_STATUS_NET_CONNECTING;
                dialog_module_instance()->speaker_play_prompt_tone(PROMPT_POWER_ON);
                break;

            case SYSTEM_EVENT_NET_CONNECTED:
                LOGI("net connected ok:%d\r\n", ota_instance()->isActive());
                if(!ota_instance()->isActive())
                {
                    module->m_system_status = SYSTEM_STATUS_NET_CONNECTED;
                    ota_instance()->super.start();
                }
                else
                {
                    module->m_system_status = SYSTEM_STATUS_DEV_ACTIVED;
                }
                break;

            case SYSTEM_EVENT_NET_CONNECT_FAIL:
                LOGI("net connect fail\r\n");
                if(SYSTEM_STATUS_NET_CONFIG_DONE <= module->m_system_status){
                    protocol_instance()->super.stop();
                    module->m_system_status = SYSTEM_STATUS_NET_DISCONNECT;
                } else {
                    dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION_FAIL);
                }
                break;

            case SYSTEM_EVENT_NET_PASSWORD_ERR:
                LOGI("net password error\r\n");
                if (SYSTEM_STATUS_NET_CONFIG_DONE > module->m_system_status){
                    dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION_FAIL);
                }
                break;

            case SYSTEM_EVENT_NET_NO_AP:
                LOGI("can't find ap\r\n");
                if(SYSTEM_STATUS_NET_CONFIG_DONE <= module->m_system_status){
                    protocol_instance()->super.stop();
                    module->m_system_status = SYSTEM_STATUS_NET_DISCONNECT;
                } else {
                    dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION_FAIL);
                }
                break;

            case SYSTEM_EVENT_DEV_ACTIVE_START:
                LOGI("active start\r\n");
                module->m_system_status = SYSTEM_STATUS_DEV_ACTIVING;
                dialog_module_instance()->speaker_play_prompt_tone(PROMPT_DEVICE_NOT_ACTIVATED);
                break;

            case SYSTEM_EVENT_DEV_ACTIVE_DONE:
                LOGI("active done\r\n");
                if (module->m_system_status == SYSTEM_STATUS_DEV_ACTIVING){
                    dialog_module_instance()->speaker_play_prompt_tone(PROMPT_DEVICE_ACTIVATED);
                }
                module->m_system_status = SYSTEM_STATUS_DEV_ACTIVED;

                //websocket模块init 这里再初始化，连接云端的url token等参数需要从active流程获取
                protocol_instance()->super.init();
                break;

            case SYSTEM_EVENT_DIALOG_START:
#if (CONFIG_AUDIO_PLAYER)
                app_audio_player_pause();
#endif

                if(SYSTEM_STATUS_DEV_ACTIVED <= module->m_system_status 
                && module->m_system_status < SYSTEM_STATUS_SERV_CONNECT_START)
                {
                    LOGI("dialog start\r\n");
                    module->send_msg_by_event(SYSTEM_EVENT_SERV_CONNECT_START);
                    module->m_is_last = false;
                    module->m_system_status = SYSTEM_STATUS_SERV_CONNECT_START;
                    bk_wifi_sta_pm_disable();
                }
                else if(SYSTEM_STATUS_PLAYING == module->m_system_status)
                {
                    LOGI("playing state, first abort playing\r\n");
                    module->send_msg_by_event(SYSTEM_EVENT_DIALOG_ABORT);
                }
                else if(SYSTEM_STATUS_SERV_CONNECT_OK == module->m_system_status){
                    protocol_instance()->sendWakeJson((uint8_t*)DEV_WAKE_NAME);
                }
                else
                {
                    LOGE("error status:%d\r\n", module->m_system_status);
                }
                break;

            case SYSTEM_EVENT_SERV_CONNECT_START:                  //云连接开始
                LOGI("server connect start\r\n");
                if(!protocol_instance()->m_is_start){
                    protocol_instance()->super.start();
                }
                else{
                    protocol_instance()->super.stop();
                    protocol_instance()->super.start();
                }
                
                module->m_system_status = SYSTEM_STATUS_SERV_CONNECTING;
                break;

            case SYSTEM_EVENT_SERV_CONNECT_OK:                  //网络连接上
                LOGI("connect ok\r\n");
                if(module->m_system_status == SYSTEM_STATUS_SERV_CONNECTING){
                    protocol_instance()->sendWakeJson((uint8_t*)DEV_WAKE_NAME);
                }
                pet_scene_handle_event(PET_EVENT_CLOUD_CONNECTED);
                module->m_system_status = SYSTEM_STATUS_SERV_CONNECT_OK;
                break;

            case SYSTEM_EVENT_SERV_NULL:                  //云连接断开
                if(SYSTEM_STATUS_NET_CONFIG_DONE <= module->m_system_status){
                    LOGI("server disconnect\r\n");
                    /*如果是网络正常的情况下，云端断开连接，重新进行语音唤醒检测*/
                    protocol_instance()->super.stop();
                    module->m_system_status = SYSTEM_STATUS_SERV_NULL;
                    pet_scene_handle_event(PET_EVENT_CLOUD_ERROR);
                    bk_wifi_sta_pm_enable();
                }
                break;

            case SYSTEM_EVENT_DIALOG_ABORT:
                LOGI("status:%d, is_last:%d\r\n", module->m_system_status, module->m_is_last);
                pet_scene_handle_event(PET_EVENT_AUDIO_ABORT);
                if(SYSTEM_STATUS_PLAYING ==  module->m_system_status && !(module->m_is_last)){
                    module->m_system_status = SYSTEM_STATUS_RECORDING;
                }
                break;

            case SYSTEM_EVENT_RECORD_START:                    //开始录音
                LOGI("record start:%d\r\n", module->m_system_status);
                if (SYSTEM_STATUS_RECORDING == module->m_system_status) {
                    LOGW("skip duplicate record start\r\n");
                    break;
                }
                if (SYSTEM_STATUS_RECORDING_DONE == module->m_system_status) {
                    LOGW("skip record start after recording done\r\n");
                    break;
                }
                pet_brain_get_snapshot(&snapshot);
                if (snapshot.privacy) {
                    LOGW("pet voice blocked by privacy\r\n");
                    pet_scene_handle_event(PET_EVENT_AUDIO_LISTEN_START);
                    break;
                }
                if (SYSTEM_STATUS_PLAYING ==  module->m_system_status) {
                    dialog_module_instance()->speaker_play_abort();
                    protocol_instance()->sendAbortListen((uint8_t*)"detect");
                }
                if(_voice_status_can_start_listen(module->m_system_status))
                {
                    if (BK_OK != _recover_voice_stream_before_listen("manual")) {
                        LOGE("recover voice stream before listen start failed\r\n");
                        pet_scene_handle_event(PET_EVENT_CLOUD_ERROR);
                        break;
                    }
                    pet_scene_handle_event(PET_EVENT_AUDIO_LISTEN_START);
                    protocol_instance()->sendStartListen((uint8_t*)"manual");
                    module->m_system_status = SYSTEM_STATUS_RECORDING;
                }
                break;

            case SYSTEM_EVENT_RECORD_END:                      //录音结束
                LOGI("record end\r\n");
                if (SYSTEM_STATUS_RECORDING == module->m_system_status) {
                    pet_scene_handle_event(PET_EVENT_AUDIO_LISTEN_STOP);
                    protocol_instance()->sendStopListen();
                    module->m_system_status = SYSTEM_STATUS_RECORDING_DONE;
                } else {
                    LOGW("skip record end in status:%d\r\n", module->m_system_status);
                }
                break;

            case SYSTEM_EVENT_ASR_TIMEOUT:
                break;

            case SYSTEM_EVENT_PLAY_START:                      //开始播放
                if(last_status != SYSTEM_EVENT_PLAY_START)
                {
                    LOGI("play start:%d\r\n", module->m_system_status);

                    if(SYSTEM_STATUS_SERV_CONNECT_OK <= module->m_system_status && module->m_system_status <= SYSTEM_STATUS_PLAYING){
                        pet_scene_handle_event(PET_EVENT_AUDIO_TTS_START);
                        dialog_module_instance()->speaker_pa_control(true);
                        /*开始播放语音数据*/
                        module->m_system_status = SYSTEM_STATUS_PLAYING;
                    }
                }
                break;

            case SYSTEM_EVENT_PLAY_END:                        //播放结束
                LOGI("play end:%d\r\n", module->m_system_status);
                pet_scene_handle_event(PET_EVENT_AUDIO_TTS_STOP);
                if(_voice_status_can_auto_listen_after_play(module->m_system_status)){
                    if(module->m_is_last){
                        module->send_msg_by_event(SYSTEM_EVENT_IDLE);
                    }
                    else{
                        if (BK_OK != _recover_voice_stream_before_listen("auto")) {
                            LOGE("recover voice stream before auto listen failed\r\n");
                            pet_scene_handle_event(PET_EVENT_CLOUD_ERROR);
                            break;
                        }
                        protocol_instance()->sendStartListen((uint8_t*)"auto");
                        module->m_system_status = SYSTEM_STATUS_RECORDING;
                        //_sm_wait_aud();
                        //dialog_module_instance()->speaker_pa_control(false);
                    }
                } else {
                    LOGW("skip auto listen after play end in status:%d\r\n", module->m_system_status);
                }
                break;

            case SYSTEM_EVENT_LAST_SENTENCE:                    //这里仅在应用场景说完byebye仍要播放最后一句话再使用
                LOGI("wait for last sentence:%d\r\n", module->m_system_status);
                if(SYSTEM_STATUS_SERV_CONNECT_OK <= module->m_system_status && module->m_system_status <= SYSTEM_STATUS_PLAYING){
                    module->m_is_last = true;
                }
                break;

            case SYSTEM_EVENT_DIALOG_END:
                LOGI("dialog end\r\n");
                if(module->m_system_status >= SYSTEM_STATUS_DEV_ACTIVED){
                    module->m_system_status = SYSTEM_STATUS_LOCAL_ASR_DIALOG_END;
                    protocol_instance()->sendAbortListen((uint8_t*)"detect");
                    dialog_module_instance()->speaker_play_abort();
                    module->send_msg_by_event(SYSTEM_EVENT_IDLE);
                }
                else{
                    LOGE("error status:%d\r\n", module->m_system_status);
                }
                break;

            case SYSTEM_EVENT_IDLE:
                LOGI("enter idle\r\n");
                if(SYSTEM_STATUS_SERV_CONNECTING <= module->m_system_status && module->m_system_status <= SYSTEM_STATUS_PLAYING){
                    LOGI("start stop\r\n");
                    protocol_instance()->super.stop();
                    bk_wifi_sta_pm_enable();
                    module->m_system_status = SYSTEM_STATUS_IDLE;
                }
                break;
            case SYSTEM_EVENT_UI_DISP_EMOJI:
                if (msg.param != NULL) {
                    pet_brain_emit_emotion((char *)msg.param);
                }
                break;
            default:
                LOGI("other event:%d\r\n", msg.event);
                break;
        }

        extern void system_event_cb(system_event_e event, uint8_t *data);
        system_event_cb(msg.event, msg.param);
        if((msg.param != 0)){
            LOGI("free data\r\n");
            os_free(msg.param);
        }

        if(last_status != msg.event)
        {
            last_status = msg.event;
        }
    }

    module->m_task_state = TASK_STATE_INIT;
    module->m_thread_hd = NULL;
    rtos_set_semaphore(&module->m_sem);
    rtos_delete_thread(NULL);
}

static s32 _sm_init(void)
{
    system_manager_module_t *module = system_manager_instance();
    bk_err_t ret = BK_FAIL;

    do{
        if(module->m_is_init)
        {
            ret = BK_OK;
            break;
        }

        ret = rtos_init_semaphore_ex(&module->m_sem, 1, 0);
        if(BK_OK != ret)
        {
            LOGE("init sem fail\r\n");
            break;
        }
        
        ret = rtos_init_queue(&module->m_queue, SYSTEM_MANAGER_QUEUE_NAME,  sizeof(sysMsg_t), SYSTEM_MANAGER_QUEUE_SIZE);
        if(BK_OK != ret)
        {
            LOGE("init queue fail\r\n");
            break;
        }

        LOGI("init success\r\n");
        module->m_is_init = true;
        ret = BK_OK;
    }while(0);

    if(!module->m_is_init)
    {
        if(module->m_sem)
        {
            rtos_deinit_semaphore(&module->m_sem);
            module->m_sem = NULL;
        }
        
        if(module->m_queue)
        {
            rtos_deinit_queue(&module->m_queue);
            module->m_queue = NULL;
        }
    }
    return ret;
}

static s32 _sm_start(void)
{
    system_manager_module_t *module = system_manager_instance();
    bk_err_t ret = BK_FAIL;
    sysMsg_t msg = {0};

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        LOGI("start enter\r\n");
        ret = rtos_create_thread(&module->m_thread_hd, SYSTEM_MANAGER_TASK_PRIORITY, SYSTEM_MANAGER_TASK_NAME, _manager_task, SYSTEM_MANAGER_TASK_SIZE, NULL);
        if(BK_OK != ret)
        {
            LOGE("create thread fail\r\n");
            break;
        }

        ret = rtos_get_semaphore(&module->m_sem, BEKEN_NEVER_TIMEOUT);
        if (BK_OK != ret)
        {
            LOGE("wait sem fail\r\n");
            break;
        }

        msg.event = SYSTEM_EVENT_INIT;
        module->send_msg(&msg);
        LOGI("start success\r\n");
    }while(0);

    return ret;
}

static s32 _sm_stop(void)
{
    system_manager_module_t *module = system_manager_instance();
    bk_err_t ret = BK_FAIL;

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        LOGI("stop enter\r\n");
        module->m_task_state = TASK_STATE_STOP;
        ret = rtos_get_semaphore(&module->m_sem, BEKEN_NEVER_TIMEOUT);
        if (BK_OK != ret)
        {
            LOGE("wait sem fail\r\n");
            break;
        }

        LOGI("stop success\r\n");
    }while(0);
    
    return ret;
}

static s32 _sm_deinit(void)
{
    system_manager_module_t *module = system_manager_instance();
    bk_err_t ret = BK_FAIL;

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        LOGI("deinit enter\r\n");
        if(TASK_STATE_INIT != module->m_task_state)
        {
            LOGE("task don't stop:%d\r\n", module->m_task_state);
            break;
        }

        if(module->m_sem)
        {
            rtos_deinit_semaphore(&module->m_sem);
            module->m_sem = NULL;
        }
        
        if(module->m_queue)
        {
            rtos_deinit_queue(&module->m_queue);
            module->m_queue = NULL;
        }
        LOGI("deinit success\r\n");
        module->m_is_init = false;
        ret = BK_OK;
    }while(0);

    return ret;
}

static void _sm_send_msg(sysMsg_t* msg)
{
    system_manager_module_t *module = system_manager_instance();

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        if (module->m_queue) 
        {
            if (kNoErr != rtos_push_to_queue(&module->m_queue, msg, BEKEN_NO_WAIT)) 
            {
                LOGE("send msg fail \n");
                break;
            }
        }
    }while(0);

    return ;
}

static void _sm_send_msg_by_event(system_event_e event)
{
    system_manager_module_t *module = system_manager_instance();
    sysMsg_t msg = {0};

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        msg.event = event;
        if (module->m_queue) 
        {
            if (kNoErr != rtos_push_to_queue(&module->m_queue, &msg, BEKEN_NO_WAIT)) 
            {
                LOGE("send msg fail \n");
                break;
            }
        }
    }while(0);

    return ;
}


system_status_e _sm_get_system_status(void)
{
    system_manager_module_t *module = system_manager_instance();
    system_status_e status = SYSTEM_STATUS_NULL;

    do{
        if(!module->m_is_init)
        {
            LOGE("module don't init\r\n");
            break;
        }

        status = module->m_system_status;
    }while(0);

    return status;
}

char *_get_system_status_string(system_status_e system_status)
{
    char *pstr = NULL;
    
    switch(system_status)
    {
        case SYSTEM_STATUS_INIT:
            pstr = "SYSTEM_STATUS_INIT";
            break;

        case SYSTEM_STATUS_NET_NULL:
            pstr = "SYSTEM_STATUS_NET_NULL";
            break;

        case SYSTEM_STATUS_NET_CONFIGING:
            pstr = "SYSTEM_STATUS_NET_CONFIGING";
            break;

        case SYSTEM_STATUS_NET_DISCONNECT:
            pstr = "SYSTEM_STATUS_NET_DISCONNECT";
            break;

        case SYSTEM_STATUS_NET_CONFIG_DONE:
            pstr = "SYSTEM_STATUS_NET_CONFIG_DONE";
            break;

        case SYSTEM_STATUS_NET_CONNECTING:
            pstr = "SYSTEM_STATUS_NET_CONNECTING";
            break;

        case SYSTEM_STATUS_NET_CONNECTED:
            pstr = "SYSTEM_STATUS_NET_CONNECTED";
            break;

        case SYSTEM_STATUS_DEV_ACTIVING:
            pstr = "SYSTEM_STATUS_DEV_ACTIVING";
            break;

        case SYSTEM_STATUS_DEV_ACTIVED:
            pstr = "SYSTEM_STATUS_DEV_ACTIVED";
            break;

        case SYSTEM_STATUS_IDLE:
            pstr = "SYSTEM_STATUS_IDLE";
            break;

        case SYSTEM_STATUS_SERV_NULL:
            pstr = "SYSTEM_STATUS_SERV_NULL";
            break;

        case SYSTEM_STATUS_SERV_CONNECT_START:
            pstr = "SYSTEM_STATUS_SERV_CONNECT_START";
            break;

        case SYSTEM_STATUS_SERV_CONNECTING:
            pstr = "SYSTEM_STATUS_SERV_CONNECTING";
            break;

        case SYSTEM_STATUS_SERV_CONNECT_OK:
            pstr = "SYSTEM_STATUS_SERV_CONNECT_OK";
            break;

        case SYSTEM_STATUS_LOCAL_ASR_DIALOG_START:
            pstr = "SYSTEM_STATUS_LOCAL_ASR_DIALOG_START";
            break;

        case SYSTEM_STATUS_LOCAL_ASR_DIALOG_END:
            pstr = "SYSTEM_STATUS_LOCAL_ASR_DIALOG_END";
            break;

        case SYSTEM_STATUS_RECORDING:
            pstr = "SYSTEM_STATUS_RECORDING";
            break;

        case SYSTEM_STATUS_RECORDING_DONE:
            pstr = "SYSTEM_STATUS_RECORDING_DONE";
            break;

        case SYSTEM_STATUS_PLAYING:
            pstr = "SYSTEM_STATUS_PLAYING";
            break;

        case SYSTEM_STATUS_PLAYING_DONE:
            pstr = "SYSTEM_STATUS_PLAYING_DONE";
            break;

        default:
            pstr = "unknow";
            LOGI("status: %d\r\n", system_status);
            break;
    }

    return pstr;
}

static system_manager_module_t g_system_manger =
{
    .m_is_init = false,
    .m_is_last = false,
    .m_queue = NULL,
    .m_thread_hd = NULL,
    
    .super.init = _sm_init,
    .super.start = _sm_start,
    .super.stop = _sm_stop,
    .super.deinit = _sm_deinit,

    .send_msg = _sm_send_msg,
    .send_msg_by_event = _sm_send_msg_by_event,
    .get_system_status = _sm_get_system_status,
    .get_system_status_string_fmt = _get_system_status_string,
};

system_manager_module_t *system_manager_instance(void)
{
    return &g_system_manger;
}
