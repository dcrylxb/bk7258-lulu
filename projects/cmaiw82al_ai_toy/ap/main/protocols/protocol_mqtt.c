#include <os/os.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include "protocol_mqtt.h"
#include "ota_module.h"
#include "boards_common.h"
#include "dialog_module.h"
#include "mcp_server.h"
#include "system_manager.h"

#include "mbedtls/platform.h"

#include "mbedtls/ssl.h"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/version.h"
#include "mbedtls/constant_time.h"

#include "protocol.h"

#define TAG "mqt"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define MSG_READ_BUF_SIZE     4096
#define AUD_RECV_BUF_SIZE     4096
#define UDP_KEY_LEN           16
#define UDP_NONCE_LEN         16

static void _protocol_mqtt_net_null(void);
static void _protoccol_mqtt_net_ok(void);
static void _protocol_mqtt_play_end(void);

static char* mqtt_sub_topic = NULL;
static char mqt_serv[64] = {0};
static uint16_t mqt_port = 0;
static char mqt_key[32+1] = {0};
static uint8_t mqt_key_bin[UDP_KEY_LEN] = {0};
static char mqt_nonce[64+1] = {0};
static uint8_t mqt_nonce_bin[UDP_NONCE_LEN];
static char mqt_session_id[32+1] = {0};
static int udp_socket_fd = -1;
static mbedtls_aes_context aes_ctx = {0};
static uint32_t local_sequence = 0;
static uint32_t remote_sequence = 0;
static beken_thread_t _udp_recv_thr = NULL;
static beken_semaphore_t _udp_start_sem = NULL;
static beken_semaphore_t _udp_wait_thr = NULL;
static uint8_t* _udp_recv_buf = NULL;

// 辅助函数：将单个十六进制字符转换为4位数值
static int char_to_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // 无效字符
}

// 十六进制字符串转字节数组
// 返回值：成功返回解码字节数，失败返回-1
static int decode_hex_string(const char *hex_string, uint8_t *output, size_t output_max_len) {
    if (!hex_string || !output) return -1; // 空指针检查
    size_t hex_len = strlen(hex_string);
    if (hex_len % 2 != 0) return -1; // 字符串长度必须为偶数
    size_t decoded_len = hex_len / 2;
    if (decoded_len > output_max_len){
        LOGE("output buffer is not enough\r\n");
        return -1; // 输出缓冲区不足
    }

    for (size_t i = 0; i < hex_len; i += 2) {
        int high = char_to_hex(hex_string[i]);
        int low = char_to_hex(hex_string[i + 1]);
        if (high < 0 || low < 0){
            LOGE("invalid hex char: %c\r\n", hex_string[i]);
            return -1; // 无效十六进制字符
        }
        output[i / 2] = (high << 4) | low;
    }
    return decoded_len;
}

// 初始化UDP连接的函数
static int _init_udp_connection() {
    // 创建UDP套接字
    udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_fd < 0) {
        LOGE("udp socket create failed\r\n");
        return -1;
    }

    // 配置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(mqt_port);
    if (inet_pton(AF_INET, mqt_serv, &server_addr.sin_addr) <= 0) {
        LOGE("invalid server address: %s\r\n", mqt_serv);
        close(udp_socket_fd);
        udp_socket_fd = -1;
        return -1;
    }

    // 连接到服务器（UDP的连接只是记录服务器地址，不建立实际连接）
    if (connect(udp_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("udp connect failed\r\n");
        close(udp_socket_fd);
        udp_socket_fd = -1;
        return -1;
    }

    LOGI("UDP init success\r\n");
    return 0;
}
// 使用UDP接收数据的函数
static int _udp_recv_data(char *buf, size_t buf_len) {
    if (udp_socket_fd < 0) {
        LOGE("udp socket not init\r\n");
        return -1;
    }
    ssize_t recv_bytes = recv(udp_socket_fd, buf, buf_len, 0);
    if (recv_bytes < 0) {
        LOGE("udp recv data failed\r\n");
        return -1;
    }
    LOGD("recv %zd bytes udp data success\r\n", recv_bytes);
    return recv_bytes;
}

// 使用UDP发送数据的函数
static int _udp_send_data(const char *data, size_t len) {
    if (udp_socket_fd < 0) {
        LOGE("udp socket not init\r\n");
        return -1;
    }

    ssize_t sent_bytes = send(udp_socket_fd, data, len, 0);
    if (sent_bytes < 0) {
        LOGE("udp send data failed\r\n");
        return -1;
    }

    // LOGD("send %zd bytes udp data success\r\n", sent_bytes);
    return 0;
}

// 关闭UDP连接的函数
static int _close_udp_connection() 
{
    if (udp_socket_fd >= 0) {
        close(udp_socket_fd);
        udp_socket_fd = -1;
        LOGI("udp connection close\r\n");
    }
    return 0;
}

static void _parse_server_hello(cJSON* root)
{
    int ret = 0;
    do{
        cJSON* transport = cJSON_GetObjectItem(root, "transport");
        if (transport == NULL) {
            LOGE("transport is null\r\n");
            break;
        }

        cJSON* session_id = cJSON_GetObjectItem(root, "session_id");
        if (session_id == NULL) {
            LOGE("session_id is null\r\n");
            break;
        }
        else{
            snprintf(mqt_session_id, sizeof(mqt_session_id), "%s", session_id->valuestring);
        }

        cJSON* udp = cJSON_GetObjectItem(root, "udp");
        if (udp == NULL) {
            LOGE("udp is null\r\n");
            break;
        }

        cJSON* server = cJSON_GetObjectItem(udp, "server");
        if (server == NULL) {
            snprintf(mqt_serv, sizeof(mqt_serv), "%s", XIAOZHI_UDP_HOST);
            LOGW("udp server is null, use default: %s\r\n", mqt_serv);
        }
        else{
            snprintf(mqt_serv, sizeof(mqt_serv), "%s", server->valuestring);
        }

        cJSON* port = cJSON_GetObjectItem(udp, "port");
        if (port == NULL) {
            mqt_port = XIAOZHI_UDP_PORT;
            LOGW("udp port is null, use default: %d\r\n", mqt_port);
        }
        else{
            mqt_port = port->valueint;
        }

        cJSON* key = cJSON_GetObjectItem(udp, "key");
        if (key == NULL) {
            LOGE("key is null\r\n");
            break;
        }
        else{
            snprintf(mqt_key, sizeof(mqt_key), "%s", key->valuestring);
        }

        cJSON* nonce = cJSON_GetObjectItem(udp, "nonce");
        if (nonce == NULL) {
            LOGE("nonce is null\r\n");
            break;
        }
        else{
            snprintf(mqt_nonce, sizeof(mqt_nonce), "%s", nonce->valuestring);
        }

        ret = decode_hex_string(mqt_key, mqt_key_bin, sizeof(mqt_key_bin));
        if(ret != UDP_KEY_LEN){
            LOGE("decode hex string failed, ret: %d\r\n", ret);
            break;
        }

        ret = decode_hex_string(mqt_nonce, mqt_nonce_bin, sizeof(mqt_nonce_bin));
        if(ret != UDP_NONCE_LEN){
            LOGE("decode hex string failed, ret: %d\r\n", ret);
            break;
        }
#if 0
        LOGI("mqt_key_bin: ");
        for (int i = 0; i < sizeof(mqt_key_bin); i++) {
            LOGI("%02x", mqt_key_bin[i]);
        }
        LOGI("\r\n");   

        LOGI("mqt_nonce_bin: ");
        for (int i = 0; i < sizeof(mqt_nonce_bin); i++) {
            LOGI("%02x", mqt_nonce_bin[i]);
        }
        LOGI("\r\n");
#endif  

        mbedtls_aes_init(&aes_ctx);
        ret = mbedtls_aes_setkey_enc(&aes_ctx, (const unsigned char*)mqt_key_bin, 128);
        if(0 != ret){
            LOGE("aes setkey failed, ret: %d\r\n", ret);
            break;
        }

        LOGI("mqt_serv: %s, mqt_port: %d, mqt_key: %s, mqt_nonce: %s\r\n", mqt_serv, mqt_port, mqt_key, mqt_nonce);
        if(0 == _init_udp_connection()){
            //_udp_send_data("hello", 5);
            _protoccol_mqtt_net_ok();
        }
        else{
            _protocol_mqtt_net_null();
            LOGE("udp init failed\r\n");
            break;
        }
        
        local_sequence = 0;
        remote_sequence = 0;
        rtos_set_semaphore(&_udp_start_sem);
        rtos_set_semaphore(&_udp_wait_thr);
    }while(0);
}

static void _protoccol_mqtt_net_ok(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_SERV_CONNECT_OK;
    system_manager_instance()->send_msg(&msg);
}

static void _protocol_mqtt_net_null(void)
{
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_SERV_NULL;
    system_manager_instance()->send_msg(&msg);
}

static void _protocol_mqtt_play_start(void)
{
    /*send audio start event*/
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_PLAY_START;
    system_manager_instance()->send_msg(&msg);
}

static void _protocol_mqtt_play_end(void)

{
    /*send audio end event*/
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_PLAY_END;
    system_manager_instance()->send_msg(&msg);
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
        if(!mqtt_module_instance()->is_start){
            _parse_server_hello(root);
        }
    }
    else if (os_strcmp(type->valuestring, "tts") == 0) {
        state = cJSON_GetObjectItem(root, "state");
        if (os_strcmp(state->valuestring, "start") == 0) {
            /*send audio start event*/
            _protocol_mqtt_play_start();
        } 
        else if (os_strcmp(state->valuestring, "stop") == 0) {
            /*send audio stop event*/
            _protocol_mqtt_play_end();
        }
        else if (os_strcmp(state->valuestring, "sentence_start") == 0) {
            text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                LOGI("<< %s\r\n", text->valuestring);
                /*display text*/
                send_disp_text(text->valuestring);
            }
            if(system_manager_instance()->m_system_status != SYSTEM_STATUS_PLAYING){
                _protocol_mqtt_play_start();
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
        cJSON *commands = cJSON_GetObjectItem(root, "commands");
        if (commands != NULL) {
            /*control iot devices*/
        }
    }
#endif
    else if(os_strcmp(type->valuestring, "goodbye") == 0){
        _protocol_mqtt_net_null();
    }
    else if(os_strcmp(type->valuestring, "error") == 0){
        _protocol_mqtt_net_null();
    }

    cJSON_Delete(root);
}

static void recv_audio_data_handle(uint8_t* data, int len)
{
    if(SYSTEM_STATUS_PLAYING == system_manager_instance()->m_system_status){
        dialog_module_instance()->write_speaker_data(data, len);
    }
}

static void _protocol_udp_recv_cb(uint8_t* data, int len)
{
    int ret = 0;
    uint32_t sequence = 0;
    uint8_t *opus_buf = NULL;
    size_t real_size = len - UDP_NONCE_LEN;
    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};
    uint8_t* nonce = data;

    LOGD("recv %d bytes data\r\n", len);
    do{
        opus_buf = psram_malloc(len);
        if(opus_buf == NULL){
            LOGE("psram malloc err\r\n");
            break;
        }

        if(len < UDP_NONCE_LEN){
            LOGE("err packet! udp data len is less than nonce len, len:%d\r\n", len);
        }
        if(data[0] != 0x01){
            LOGE("err packet! udp data first byte is not 0x01, data[0]:0x%x\r\n", data[0]);
        }

        sequence = ntohl(*(uint32_t*)&data[12]);
        if (sequence < remote_sequence) {
            LOGW("Received audio packet with old sequence: %lu, expected: %lu", sequence, remote_sequence);
            break;
        }
        if (sequence != remote_sequence + 1) {
            LOGW( "Received audio packet with wrong sequence: %lu, expected: %lu", sequence, remote_sequence + 1);
        }

        ret = mbedtls_aes_crypt_ctr(&aes_ctx, real_size, &nc_off, nonce, stream_block, data + UDP_NONCE_LEN, opus_buf);
        if(ret != 0){
            LOGE("aes decrypt err, ret:%d\r\n", ret);
            break;
        }

        recv_audio_data_handle(opus_buf, real_size);
        remote_sequence = sequence;
    }while(0);
    
    if(opus_buf != NULL){
        os_free(opus_buf);
    }

}

static void _udp_recv_task(beken_thread_arg_t arg)
{
    int data_len = 0;
    rtos_set_semaphore(&_udp_wait_thr);
    rtos_get_semaphore(&_udp_start_sem, BEKEN_NEVER_TIMEOUT);
    while(1){
        data_len = _udp_recv_data((char*)_udp_recv_buf, AUD_RECV_BUF_SIZE);
        if(data_len > 0){
            _protocol_udp_recv_cb(_udp_recv_buf, data_len);
        }
    }
}

static void mqtt_sub_callback(MQTT_CLIENT_T *c, MessageData *msg_data)
{
    char *payload = NULL;
    
    LOGI("mqtt_sub_callback len %d\r\n",msg_data->message->payloadlen);
    payload = psram_malloc(msg_data->message->payloadlen + 1);
    if (payload == NULL) {
        LOGE("psram_malloc fail\r\n");
        return;
    }
    os_memcpy(payload, msg_data->message->payload, msg_data->message->payloadlen);
    payload[msg_data->message->payloadlen] = '\0';
    LOGI("payload: %s\r\n", payload);

    cJSON *root = cJSON_Parse(payload);
    if (root != NULL) {
        recv_cjson_handle(root);
    } else {
        LOGE("Failed to parse JSON data\r\n");
    }

    psram_free(payload);
    os_memset(msg_data->message->payload, 0, msg_data->message->payloadlen);
    return;
}

static void mqtt_connect_callback(MQTT_CLIENT_T *c)
{
    LOGI("mqtt_connect_callback\r\n");
    return;
}

static void mqtt_online_callback(MQTT_CLIENT_T *c)
{
    LOGI("mqtt_online_callback\r\n");
    return;
}

static void mqtt_offline_callback(MQTT_CLIENT_T *c)
{
    LOGI("mqtt_offline_callback\r\n");
    _protocol_mqtt_net_null();
    return;
}


static void _mqtt_send_text(uint8_t* text)
{
    int ret = 0;
    mqtt_module_t *mqtt_module = mqtt_module_instance();
    MQTT_CLIENT_T *mqtt_client = &mqtt_module->mqtt_client;
    MQTTMessage message;
    const char *msg_str = (char*)text;
    const char *topic = ota_instance()->getPubTopic();
    LOGI("%s\r\n", topic);
    LOGI("%s\r\n", msg_str);

    if(mqtt_client == NULL){

        LOGE("mqtt_client is NULL\r\n");
        return;
    }

    message.qos = 0;
    message.retained = 0;
    message.payload = (void *)msg_str;
    message.payloadlen = os_strlen(msg_str);
    LOGI("message.payloadlen:%d\r\n",message.payloadlen);

    ret = mqtt_publish_with_topic(mqtt_client, topic, &message);
    if(ret != 0){
        LOGE("mqtt_publish_with_topic failed\r\n");
        _protocol_mqtt_net_null();
    }
}

static void _mqtt_send_audio(uint8_t* opus, int payload_len)
{
    int ret = 0;
    uint8_t* aes_opus = NULL;
    uint8_t* nonce = NULL;
    //time_t timest = time(NULL);  // 获取当前时间戳
    uint32_t timest = rtos_get_tick_count();
    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    do{
        aes_opus = psram_malloc(payload_len*sizeof(char) + UDP_NONCE_LEN);
        if(aes_opus == NULL){
            LOGE("psram malloc failed\r\n");
            _protocol_mqtt_net_null();
            break;
        }
        os_memset(aes_opus, 0, payload_len*sizeof(char) + UDP_NONCE_LEN);

        nonce = (uint8_t*)psram_malloc(UDP_NONCE_LEN*sizeof(uint8_t));
        if(nonce == NULL){
            LOGE("psram malloc failed\r\n");
            _protocol_mqtt_net_null();
            break;
        }

        memcpy(nonce, mqt_nonce_bin, UDP_NONCE_LEN);
        *(uint16_t*)&nonce[2] = htons((uint16_t)payload_len);
        *(uint32_t*)&nonce[8] = htonl((uint32_t)timest);  // 将时间戳转换为网络字节序并赋值
        *(uint32_t*)&nonce[12] = htonl((uint32_t)(++local_sequence));  // 自增序列号
        #if 0
        LOGI("seq:%d \r\n", local_sequence);
        LOGI("[nonce_seq] %d\r\n", ntohl(*(uint32_t*)&nonce[12]));
        LOGI("non2:%02X, %02X\r\n",
             (uint8_t)nonce[2], (uint8_t)nonce[3], (uint8_t)nonce[4], (uint8_t)nonce[5]);
        LOGI("non8:%02X, %02X, %02X, %02X\r\n",
             (uint8_t)nonce[8], (uint8_t)nonce[9], (uint8_t)nonce[10], (uint8_t)nonce[11]);
        LOGI("non12:%02X, %02X, %02X, %02X\r\n",
             (uint8_t)nonce[12], (uint8_t)nonce[13], (uint8_t)nonce[14], (uint8_t)nonce[15]);
        #endif

        memcpy(aes_opus, nonce, UDP_NONCE_LEN);
        ret = mbedtls_aes_crypt_ctr(&aes_ctx, payload_len, &nc_off, nonce, stream_block, opus, aes_opus + UDP_NONCE_LEN);
        if(ret != 0){
            LOGE("mbedtls_aes_crypt_ctr failed\r\n");
            break;
        }

        #if 0
        LOGI("seq:%d \r\n", local_sequence);
        LOGI("[nonce_seq] %d\r\n", ntohl(*(uint32_t*)&nonce[12]));
        LOGI("non2:%02X, %02X\r\n",
             (uint8_t)nonce[2], (uint8_t)nonce[3], (uint8_t)nonce[4], (uint8_t)nonce[5]);
        LOGI("non8:%02X, %02X, %02X, %02X\r\n",
             (uint8_t)nonce[8], (uint8_t)nonce[9], (uint8_t)nonce[10], (uint8_t)nonce[11]);
        LOGI("non12:%02X, %02X, %02X, %02X\r\n",
             (uint8_t)nonce[12], (uint8_t)nonce[13], (uint8_t)nonce[14], (uint8_t)nonce[15]);
        #endif

        ret = _udp_send_data((char*)aes_opus, payload_len + UDP_NONCE_LEN);
        if(ret != 0){
            LOGE("_udp_send_data failed\r\n");
            _protocol_mqtt_net_null();
            break;
        }
        // LOGI("send %zd bytes udp data success\r\n", payload_len + UDP_NONCE_LEN);
    }while(0);

    os_free(aes_opus);
    os_free(nonce);
}

static char* _mqtt_get_session_id(void)
{
    return mqt_session_id;
}

static int _mqtt_init(void)
{
    int ret = 0;
    mqtt_module_t *mqtt_module = mqtt_module_instance();
    MQTT_CLIENT_T *mqtt_client = &mqtt_module->mqtt_client;
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;
    os_memset(&mqtt_module->mqtt_client, 0, sizeof(MQTT_CLIENT_T));

    do{
        /* config connect param */
        memcpy(&mqtt_client->condata, &condata, sizeof(condata));
        mqtt_client->uri = ota_instance()->getUrl();
        mqtt_client->condata.clientID.cstring = ota_instance()->getClientId();
        mqtt_client->condata.keepAliveInterval = 60;
        mqtt_client->condata.cleansession = 1;
        mqtt_client->condata.username.cstring = ota_instance()->getUsername();
        mqtt_client->condata.password.cstring = ota_instance()->getPassword();
        mqtt_client->condata.struct_version = 3;

        LOGI("uri:%s\r\n",mqtt_client->uri);
        LOGI("clientID:%s\r\n",mqtt_client->condata.clientID.cstring);
        LOGI("username:%s\r\n",mqtt_client->condata.username.cstring);
        LOGI("password:%s\r\n",mqtt_client->condata.password.cstring);
        /* config MQTT will param. */
        #if 0
        mqtt_client->condata.willFlag = 1;
        mqtt_client->condata.will.qos = 0;
        mqtt_client->condata.will.retained = 0;
        mqtt_client->condata.will.topicName.cstring = ota_instance()->getPubTopic();
        //mqtt_client->condata.will.message.cstring = MQTT_WILLMSG;
        #endif

        /* malloc buffer. */
        mqtt_client->buf_size = mqtt_client->readbuf_size = MSG_READ_BUF_SIZE;
        mqtt_client->buf = psram_malloc(mqtt_client->buf_size);
        // 增加内存分配失败检查
        if (mqtt_client->buf == NULL) {
            LOGE("Failed to allocate memory for mqtt_client->buf");
            ret = -1;
            break;
        }
        mqtt_client->readbuf = psram_malloc(mqtt_client->readbuf_size);
        // 增加内存分配失败检查
        if (mqtt_client->readbuf == NULL) {
            LOGE("Failed to allocate memory for mqtt_client->readbuf");
            os_free(mqtt_client->buf);
            ret = -1;
            break;
        }
        LOGI("url: %s\r\n",ota_instance()->getUrl());
        /* set event callback function */
        mqtt_client->connect_callback = mqtt_connect_callback;
        mqtt_client->online_callback = mqtt_online_callback;
        mqtt_client->offline_callback = mqtt_offline_callback;

        /* set subscribe table and event callback */
        mqtt_sub_topic = psram_malloc(128);
        if(mqtt_sub_topic == NULL){
            LOGE("Failed to allocate memory for mqtt_sub_topic");
            ret = -1;
            break;
        }
        //snprintf(mqtt_sub_topic, 128, "devices/p2p/{%s}", board_instance()->getSubMac());
        //snprintf(mqtt_sub_topic, 128, "devices/p2p/c8_47_8c_26_ee_93");
        //mqtt_client->messageHandlers[0].topicFilter = mqtt_sub_topic;
        //mqtt_client->messageHandlers[0].callback = mqtt_sub_callback;
        //mqtt_client->messageHandlers[0].qos = 0;
        /* set default subscribe event callback */
        mqtt_client->defaultMessageHandler = mqtt_sub_callback;

        ret = rtos_init_semaphore_ex(&_udp_start_sem, 1, 0);
        if(ret != 0){
            LOGE("rtos_init_semaphore_ex failed\r\n");
            break;
        }

        ret = rtos_init_semaphore_ex(&_udp_wait_thr, 1, 0);
        if(ret != 0){
            LOGE("rtos_init_semaphore_ex failed\r\n");
            break;
        }

        _udp_recv_buf = psram_malloc(AUD_RECV_BUF_SIZE);
        if(_udp_recv_buf == NULL){
            LOGE("Failed to allocate memory for _udp_recv_buf");
            ret = -1;
            break;
        }

        ret = mqtt_module_instance()->super.start();
        if(ret != 0){
            LOGE("mqtt_module_instance()->super.start failed\r\n");
            break;
        }
        
        _protocol_mqtt_play_end();
    }while(0);
    
    return ret;
}

static int _mqtt_start(void)
{
    int ret = 0;
    mqtt_module_t *mqtt_module = mqtt_module_instance();

    do{
        if(mqtt_module->is_start){
            return 0;
        }

        ret = rtos_create_thread(&_udp_recv_thr, 3,"udp_recv_thr", _udp_recv_task,  4096, NULL);
        if(ret != 0){
            LOGE("rtos_thread_create failed\r\n");
        }
        rtos_get_semaphore(&_udp_wait_thr, BEKEN_NEVER_TIMEOUT);

        ret = paho_mqtt_start(&mqtt_module_instance()->mqtt_client);
        if(ret != 0){
            LOGE("paho_mqtt_start failed\r\n");
        }
        
        ret = rtos_get_semaphore(&_udp_wait_thr, 10000);
        if(ret != 0){
            LOGE("server hello timeout\r\n");
        }
        
        mqtt_module->is_start = true;
    }while(0);

    return ret;
}

static int _mqtt_stop(void)
{
    int ret = 0;
    mqtt_module_t *mqtt_module = mqtt_module_instance();
    if(mqtt_module->is_start){
        ret = paho_mqtt_stop(&mqtt_module->mqtt_client);
        if(ret != 0){
            LOGE("paho_mqtt_stop failed\r\n");
        }
        mqtt_module->is_start = false;
        mbedtls_aes_free(&aes_ctx);
        _close_udp_connection();
        rtos_delete_thread(&_udp_recv_thr);
        _udp_recv_thr = NULL;
        _protocol_mqtt_net_null();
    }

    return ret;
}

static int _mqtt_deinit(void)
{
    return 0;
}

static mqtt_module_t g_mqtt_manger =
{
    .is_start = false,

    .super.init = _mqtt_init,
    .super.start = _mqtt_start,
    .super.stop = _mqtt_stop,
    .super.deinit = _mqtt_deinit,
    
    .sendText = _mqtt_send_text,
    .sendAudio = _mqtt_send_audio,
    .getSessionId = _mqtt_get_session_id,
};

mqtt_module_t * mqtt_module_instance(void)
{
    return &g_mqtt_manger;
}
