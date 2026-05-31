#include <stdio.h>

#include <os/os.h>
#include <os/mem.h>
#include "protocol_websocket.h"
#include <components/webclient.h>
#include "cJSON.h"

#include "system_manager.h"
#include "ota_module.h"
#include "boards_common.h"
#include "dialog_module.h"
#include "mcp_server.h"

#define TAG "ws"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define WSS_CONN_TIME           10000
#define WSS_TX_TIME             1000
#define WS_SESSION_ID_LEN       160

static websocket_client_input_t wss_conn_cfg = {0};
static beken_semaphore_t conn_sem = NULL;
static char g_header[WEB_HEAD_LEN] = {0};
static char ws_session_id[WS_SESSION_ID_LEN] = {0};
static uint8_t* ws_recv_buf = NULL;

static void _web_get_header(void)
{
#if !CONFIG_WSS_INFO_BY_USER
    snprintf(g_header, WEB_HEAD_LEN, WEB_HEADER_TEXT, ota_instance()->getToken(), board_instance()->getMac(), board_instance()->getUid());
#else
    snprintf(g_header, WEB_HEAD_LEN, WEB_HEADER_TEXT, board_instance()->getMac(), board_instance()->getUid());
#endif
}

static void recv_aud_handle(uint8_t* opus)
{
    /*recv aud opus data , send to dialog task to play*/
}

static void _protoccol_websocket_net_null(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_SERV_NULL;
    system_manager_instance()->send_msg(&msg);
}

static void _protoccol_websocket_net_ok(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_SERV_CONNECT_OK;
    system_manager_instance()->send_msg(&msg);
}

static void _parse_server_hello(cJSON* root)
{
    cJSON* session_id = cJSON_GetObjectItem(root, "session_id");
    if (session_id == NULL || !cJSON_IsString(session_id)) {
        ws_session_id[0] = '\0';
        LOGW("server hello missing string session_id\r\n");
    }
    else {
        snprintf(ws_session_id, sizeof(ws_session_id), "%s", session_id->valuestring);
        LOGI("server session_id:%s\r\n", ws_session_id);
    }

    _protoccol_websocket_net_ok();
}

static void _protocol_websocket_play_start(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_PLAY_START;
    system_manager_instance()->send_msg(&msg);
}

static void _protocol_websocket_play_end(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_PLAY_END;
    system_manager_instance()->send_msg(&msg);
}

static void recv_audio_data_handle(uint8_t* data, int len)
{
    //sysMsg_t msg = {0};
    //msg.event = SYSTEM_EVENT_PLAYING;
    //msg.param = data;
    //system_manager_instance()->send_msg(&msg);
    if(SYSTEM_STATUS_PLAYING == system_manager_instance()->m_system_status){
        dialog_module_instance()->write_speaker_data(data, len);
    }
}

static void send_disp_text(char* text)
{
    sysMsg_t msg = {0};
    char* buf = os_malloc(os_strlen(text)+4);

    snprintf(buf, os_strlen(text)+4, "%s", text);
    
    msg.event = SYSTEM_EVENT_UI_DISP_TEXT;
    msg.param = (void*)buf;
    system_manager_instance()->send_msg(&msg);
}

static void send_disp_emoji(char* emoji)
{
    sysMsg_t msg = {0};
    char* buf = os_malloc(os_strlen(emoji)+4);

    snprintf(buf, os_strlen(emoji)+4, "%s", emoji);
    
    msg.event = SYSTEM_EVENT_UI_DISP_EMOJI;
    msg.param = (void*)buf;
    system_manager_instance()->send_msg(&msg);
}

static void recv_cjson_handle(cJSON* root)
{
    cJSON* type = NULL;
    cJSON* state = NULL;
    cJSON* text = NULL;
    cJSON* emotion = NULL;

    //can refefrence this code:
    if(root == NULL){
        LOGE("json data fmt err\r\n");
        return;
    }

    type = cJSON_GetObjectItem(root, "type");
    if (strcmp(type->valuestring, "hello") == 0) {
        _parse_server_hello(root);
    }
    else if (os_strcmp(type->valuestring, "tts") == 0) {
        state = cJSON_GetObjectItem(root, "state");
        if (os_strcmp(state->valuestring, "start") == 0) {
            /*send audio start event*/
            _protocol_websocket_play_start();
        } 
        else if (os_strcmp(state->valuestring, "stop") == 0) {
            /*send audio stop event*/
            _protocol_websocket_play_end();
        }
        else if (os_strcmp(state->valuestring, "sentence_start") == 0) {
            text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                LOGI("<< %s", text->valuestring);
                /*display text*/
                send_disp_text(text->valuestring);
            }
        }
    } 
    else if (os_strcmp(type->valuestring, "stt") == 0) {
        text = cJSON_GetObjectItem(root, "text");
        if (text != NULL) {
            /*display text*/
            send_disp_text(text->valuestring);
        }
    }
    else if (os_strcmp(type->valuestring, "llm") == 0) {
        emotion = cJSON_GetObjectItem(root, "emotion");
        if (emotion != NULL) {
            /*deal with emotion*/
            send_disp_emoji(emotion->valuestring);
        }
    }
#if CONFIG_PROTOCOL_IOT_MCP
    else if (os_strcmp(type->valuestring, "mcp") == 0){
        cJSON* payload = cJSON_GetObjectItem(root, "payload");
        mcp_server_instance()->recv_msg_cb(payload);
    }
#else
    else if (os_strcmp(type->valuestring, "iot") == 0) {
        cJSON* commands = cJSON_GetObjectItem(root, "commands");
        if (commands != NULL) {
            /*control iot devices*/
        }
    }
#endif
    else if(os_strcmp(type->valuestring, "error") == 0){
        _protoccol_websocket_net_null();
    }

    cJSON_Delete(root);
}

static void _protocol_websocket_event_handle(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data)
{
        bk_websocket_event_data_t *data = (bk_websocket_event_data_t *)event_data;
        LOGD("data from WebSocket server, event:%d op:%d\r\n", event_id, data->op_code);
        switch (event_id) {
            case WEBSOCKET_EVENT_CONNECTED:
                LOGE("Connected to WebSocket server\r\n");
                rtos_set_semaphore(&conn_sem);
                break;
            
            case WEBSOCKET_EVENT_CLOSED:
                LOGE("WEBSOCKET_EVENT_CLOSED\r\n");
                _protoccol_websocket_net_null();
                break;
                
            case WEBSOCKET_EVENT_DISCONNECTED:
                LOGE("WEBSOCKET_EVENT_DISCONNECTED\r\n");
                _protoccol_websocket_net_null();
                break;
                
            case WEBSOCKET_EVENT_DATA:
                LOGD("op:%d offset:%d data_len:%d pay_load_len:%d\r\n", data->op_code, data->payload_offset, data->data_len, data->payload_len);
                if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                    recv_audio_data_handle((uint8_t*)data->data_ptr, data->data_len);
                }
                else if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                    if(!ws_recv_buf){
                        ws_recv_buf = psram_malloc(data->payload_len + 1);
                        BK_ASSERT(ws_recv_buf);
                        os_memset(ws_recv_buf, 0, data->payload_len+1);
                    }
                    os_memcpy(ws_recv_buf+data->payload_offset, data->data_ptr, data->data_len);
                    LOGD("WS_TRANSPORT_OPCODES_TEXT\r\n");
                    if ((data->payload_offset + data->data_len) >= data->payload_len && data->data_len > 0 && data->data_ptr != NULL)
                    {
                        LOGI("recv text:%s\r\n", ws_recv_buf);
                        cJSON *root = cJSON_Parse((char*)ws_recv_buf);
                        if (root != NULL) {
                            recv_cjson_handle(root);
                        } 
                        else 
                        {
                            LOGE("Failed to parse JSON data\r\n");
                        }

                        psram_free(ws_recv_buf);
                        ws_recv_buf = NULL;
                    }
                }
                break;
                
            default:
                break;
        }
}

static void _protocl_websocket_send_audio(uint8_t* opus, int len)
{
    int ret = 0;
    pws_module_t* ws_module = protocol_websocket_instance();
    dialog_voice_diag_t diag = {0};

    if (ws_module->wss_client == NULL) {
        dialog_voice_diag_note_ws_audio_tx(len, BK_FAIL);
        LOGW("skip audio send, websocket client is NULL\r\n");
        return;
    }
    
    /*send json data*/
    ret = websocket_client_send_binary(ws_module->wss_client, (char*)opus, len, WSS_TX_TIME);
    dialog_voice_diag_note_ws_audio_tx(len, ret);
    dialog_voice_diag_get(&diag);
    if ((diag.ws_audio_tx_count % 200) == 1 || ret < 0) {
        LOGI("ws audio tx count=%u bytes=%u fail=%u last_len=%d ret=%d\r\n",
             diag.ws_audio_tx_count,
             diag.ws_audio_tx_bytes,
             diag.ws_audio_tx_fail,
             diag.ws_audio_last_len,
             diag.ws_audio_last_ret);
    }
    if(ret < 0){
        LOGE("send audio err\r\n");
        _protoccol_websocket_net_null();
    }
}

static void _protocol_websocket_send_text(uint8_t* json)
{
    LOGI("\r\n%s\r\n", json);
    int ret = 0;
    pws_module_t* ws_module = protocol_websocket_instance();

    if (ws_module->wss_client == NULL) {
        LOGW("skip text send, websocket client is NULL\r\n");
        return;
    }
    
    /*send json data*/
    ret = websocket_client_send_text(ws_module->wss_client, (char*)json, strlen((char*)json), WSS_TX_TIME);
    if(ret < 0){
        LOGE("send text err\r\n");
        _protoccol_websocket_net_null();
    }
}

static char* _websocket_get_session_id(void)
{
    return ws_session_id;
}

static int _pw_init(void)
{
    _web_get_header();
    
#if !CONFIG_WSS_INFO_BY_USER
    wss_conn_cfg.uri = ota_instance()->getUrl();
#else
    wss_conn_cfg.uri = CONFIG_WSS_SERVER_URL;
#endif

    //wss_conn_cfg.headers = WEB_HEADER_TEXT;
    wss_conn_cfg.headers = g_header;
    LOGI("url: %s, header: %s\r\n", wss_conn_cfg.uri, wss_conn_cfg.headers);

    wss_conn_cfg.ws_event_handler = _protocol_websocket_event_handle;
    rtos_init_semaphore_ex(&conn_sem, 1, 0);

    return 0;
}

static int _pw_start(void)
{
    int ret = 0;
    pws_module_t* wss_manager = protocol_websocket_instance();

    do{
        if(wss_manager->wss_client != NULL){
            LOGE("wss client already init\r\n");
            break;
        }
        
        wss_manager->wss_client = websocket_client_init(&wss_conn_cfg);
        if(websocket_client_start(wss_manager->wss_client)) {
            LOGE("wss client start fail\r\n");
            ret = -1;
            break;
        }

        ret = rtos_get_semaphore(&conn_sem, WSS_CONN_TIME);
        if(ret != 0){
            LOGE("connect time out\r\n");
            _protoccol_websocket_net_null();
        }
    }while(0);
    
    return ret;
}

static int _pw_stop(void)
{
    int ret =0;
    pws_module_t* wss_manager = protocol_websocket_instance();

    do{
        if(wss_manager->wss_client == NULL){
            LOGE("wss client is NULL\r\n");
            break;
        }

        ret = websocket_client_destroy(wss_manager->wss_client);
        if(ret != BK_OK){
            LOGE("wss client config already destroy\r\n");
            break;
        }

        wss_manager->wss_client = NULL;
    }while(0);

    return ret;
}

static int _pw_deinit(void)
{
    rtos_deinit_semaphore(&conn_sem);

    return 0;
}

static pws_module_t g_pws_manger =
{
    .wss_client = NULL,
    
    .super.init = _pw_init,
    .super.start = _pw_start,
    .super.stop = _pw_stop,
    .super.deinit = _pw_deinit,
    .sendAudio = _protocl_websocket_send_audio,
    .sendText = _protocol_websocket_send_text,
    .getSessionId = _websocket_get_session_id,
};

pws_module_t *protocol_websocket_instance(void)
{
    return &g_pws_manger;
}
