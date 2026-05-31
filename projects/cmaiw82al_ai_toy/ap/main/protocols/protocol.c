#include "protocol.h"

#include "bk_cli.h"
#include "bk_wifi_types.h"
#include "bk_private/bk_wifi.h"

#include <components/netif.h>

#include <components/webclient.h>
#if CONFIG_PROTOCOL_USE_WSS
#include "protocol_websocket.h"
#endif
#if CONFIG_PROTOCOL_USE_MQTT
#include "protocol_mqtt.h"
#endif
#include "system_manager.h"

#include "mcp_server.h"

#if CONFIG_PROTOCOL_USE_WSS
static pws_module_t* _pws_manager = NULL;
#endif
#if CONFIG_PROTOCOL_USE_MQTT
static mqtt_module_t* _mqtt_manager = NULL;
#endif

static char* g_wss_data;

#define TAG "ptc"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

void _protocol_send_client_hello(void)
{
    cJSON * root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "type", "hello");
#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddNumberToObject(root, "version", 1);
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddNumberToObject(root, "version", 3);
#endif

#if CONFIG_PROTOCOL_IOT_MCP
    cJSON* features = cJSON_CreateObject();
    cJSON_AddTrueToObject(features, "mcp");
    cJSON_AddItemToObject(root, "features", features);
#endif

#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "transport", "websocket");
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "transport", "udp");
#endif
    cJSON* aud = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "audio_params", aud);
    cJSON_AddStringToObject(aud, "format", "opus");
    cJSON_AddNumberToObject(aud, "sample_rate", SPK_SAMPLE_RATE);
    cJSON_AddNumberToObject(aud, "channels", 1);
    cJSON_AddNumberToObject(aud, "frame_duration", SPK_FRAME_DURATION);
    
    g_wss_data = cJSON_PrintUnformatted(root);
    os_printf("%s\r\n", g_wss_data);

#if CONFIG_PROTOCOL_USE_WSS
    if(_pws_manager != NULL){
        _pws_manager->sendText((uint8_t*)g_wss_data);
    }
    else{
        LOGE("protocol not init\r\n");
    }
#elif CONFIG_PROTOCOL_USE_MQTT
    if(_mqtt_manager != NULL){
        _mqtt_manager->sendText((uint8_t*)g_wss_data);
    }
    else{
        LOGE("protocol not init\r\n");
    }
#endif

    cJSON_free(g_wss_data);
    cJSON_Delete(root);
}

static void _protocol_send_audio(uint8_t* opus, int len)
{
    /*send opus data to xiaozhi*/
    if(!protocol_instance()->m_is_init){
        LOGE("protocol not start\r\n");
        return;
    }

#if CONFIG_PROTOCOL_USE_WSS
    _pws_manager->sendAudio(opus, len);
#elif CONFIG_PROTOCOL_USE_MQTT
    _mqtt_manager->sendAudio(opus, len);
#endif
}

static void _protocol_send_wake_detected(uint8_t* wake_word)
{
    /*send detected json to 
    JSON
    {
        "session_id": "<dialog ID>",
        "type": "listen",
        "state": "detect",
        "text": "<wake_word>"
    }
    use websocket server hello session_id
    */
    if(!protocol_instance()->m_is_init){
        LOGE("protocol not start\r\n");
        return;
    }
    
    cJSON * root = cJSON_CreateObject();
#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "session_id", _pws_manager->getSessionId());
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "session_id", _mqtt_manager->getSessionId());
#endif
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "detect");
    cJSON_AddStringToObject(root, "text", (char*)wake_word);

    char* text = cJSON_PrintUnformatted(root);
    if (text) {
#if CONFIG_PROTOCOL_USE_WSS
        if(_pws_manager != NULL){
            _pws_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#elif CONFIG_PROTOCOL_USE_MQTT
        if(_mqtt_manager != NULL){
            _mqtt_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#endif
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

static void _protocol_send_start_listening(uint8_t* mode)
{
    /*
    send start listen
    {
        "session_id": "<会话ID>",
        "type": "listen",
        "state": "start",
        "mode": "<mode>"
    }
    */
    if(!protocol_instance()->m_is_init){
        LOGE("protocol not start\r\n");
        return;
    }
    cJSON * root = cJSON_CreateObject();
#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "session_id", _pws_manager->getSessionId());
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "session_id", _mqtt_manager->getSessionId());
#endif
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "mode", (char*)mode);

    char* text = cJSON_PrintUnformatted(root);
    if (text) {
#if CONFIG_PROTOCOL_USE_WSS
        if(_pws_manager != NULL){
            _pws_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#elif CONFIG_PROTOCOL_USE_MQTT
        if(_mqtt_manager != NULL){
            _mqtt_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#endif
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

static void _protocol_send_stop_listening(void)
{
    /*
    send stop listen
    {
    "session_id": "<会话ID>",
    "type": "listen",
    "state": "stop"
    }
    */
    if(!protocol_instance()->m_is_init){
        LOGE("protocol not start\r\n");
        return;
    }
    cJSON * root = cJSON_CreateObject();
#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "session_id", _pws_manager->getSessionId());
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "session_id", _mqtt_manager->getSessionId());
#endif
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "stop");

    char* text = cJSON_PrintUnformatted(root);
    if (text) {
#if CONFIG_PROTOCOL_USE_WSS
        if(_pws_manager != NULL){
            _pws_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#elif CONFIG_PROTOCOL_USE_MQTT
        if(_mqtt_manager != NULL){
            _mqtt_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#endif
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

static void _protocol_send_abort_listening(uint8_t* desc)
{
    /*
    {
    "session_id": "<会话ID>",
    "type": "abort",
    "reason": "desc" // 可选
    }
    */
    if(!protocol_instance()->m_is_init){
        LOGE("protocol not start\r\n");
        return;
    }
    cJSON * root = cJSON_CreateObject();
#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "session_id", _pws_manager->getSessionId());
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "session_id", _mqtt_manager->getSessionId());
#endif
    cJSON_AddStringToObject(root, "type", "abort");
    cJSON_AddStringToObject(root, "reason", (char*)desc);

    char* text = cJSON_PrintUnformatted(root);
    if (text) {
#if CONFIG_PROTOCOL_USE_WSS
        if(_pws_manager != NULL){
            _pws_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#elif CONFIG_PROTOCOL_USE_MQTT
        if(_mqtt_manager != NULL){
            _mqtt_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#endif
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

#if CONFIG_PROTOCOL_IOT_MCP
static void _protocol_send_mcp_message(uint8_t* msg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *payload = cJSON_Parse((char*)msg);

#if CONFIG_PROTOCOL_USE_WSS
    cJSON_AddStringToObject(root, "session_id", _pws_manager->getSessionId());
#elif CONFIG_PROTOCOL_USE_MQTT
    cJSON_AddStringToObject(root, "session_id", _mqtt_manager->getSessionId());
#endif
    cJSON_AddStringToObject(root, "type", "mcp");
    cJSON_AddItemToObject(root, "payload", payload);

    char* text = cJSON_PrintUnformatted(root);
    if (text) {
#if CONFIG_PROTOCOL_USE_WSS
        if(_pws_manager != NULL){
            _pws_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#elif CONFIG_PROTOCOL_USE_MQTT
        if(_mqtt_manager != NULL){
            _mqtt_manager->sendText((uint8_t*)text);
        }
        else{
            LOGE("protocol not init\r\n");
        }
#endif
        cJSON_free(text);
    }
    cJSON_Delete(root);
}
#endif

static int _protocol_client_start(void)
{
    /*init protcol services*/
#if CONFIG_PROTOCOL_USE_WSS
    if(_pws_manager == NULL){
        LOGE("pws_manger is null\r\n");
        return -1;
    }
    if(_pws_manager->super.start() == 0){
        LOGI("pws start success\r\n");
        _protocol_send_client_hello();
        protocol_instance()->m_is_start = true;
    }
#endif
#if CONFIG_PROTOCOL_USE_MQTT
    if(_mqtt_manager == NULL){
        LOGE("mqtt_manager is null\r\n");
        return -1;
    }
    if(_mqtt_manager->super.start() == 0){
        LOGI("mqtt start success\r\n");
        // _protocol_send_client_hello();
        protocol_instance()->m_is_start = true;
    }
#endif

    return 0;
}

static int _protocol_client_init(void)
{
#if CONFIG_PROTOCOL_IOT_MCP
        mcp_server_t* server = mcp_server_instance();
        server->super.init();
        server->super.start();
#endif

#if CONFIG_PROTOCOL_USE_WSS
    _pws_manager = protocol_websocket_instance();
    _pws_manager->super.init();
#endif

#if CONFIG_PROTOCOL_USE_MQTT
    _mqtt_manager = mqtt_module_instance();
    _mqtt_manager->super.init();
#endif

    protocol_instance()->m_is_init = true;

    return 0;
}

static int _protocol_client_stop(void)
{
    protocol_instance()->m_is_start = false;
#if CONFIG_PROTOCOL_USE_WSS
    if(_pws_manager != NULL){
        _pws_manager->super.stop();
    }
#elif CONFIG_PROTOCOL_USE_MQTT
    if(_mqtt_manager != NULL){
        _mqtt_manager->super.stop();
    }
#endif
    return 0;
}

static int _protocol_client_deinit(void)
{
    return 0;
}

static pro_module_t g_pro_manger =
{
    .m_is_init = false,
    .m_is_start = false,

    .super.init = _protocol_client_init,
    .super.start = _protocol_client_start,
    .super.stop = _protocol_client_stop,
    .super.deinit = _protocol_client_deinit,
    .sendAudio = _protocol_send_audio,
    .sendWakeJson = _protocol_send_wake_detected,
    .sendStartListen = _protocol_send_start_listening,
    .sendStopListen = _protocol_send_stop_listening,
    .sendAbortListen = _protocol_send_abort_listening,
#if CONFIG_PROTOCOL_IOT_MCP
    .sendMcpMessage = _protocol_send_mcp_message,
#endif
};
    
pro_module_t *protocol_instance(void)
{
    return &g_pro_manger;
}

