#include "modules/ota.h"

#include "bk_cli.h"
#include "bk_wifi_types.h"
#include "bk_private/bk_wifi.h"
#include "bk_private/bk_init.h"
#include "bk_private/bk_ota_private.h"
#include <components/system.h>
#include <components/netif.h>
#include <bk_websocket_client.h>
#include <components/webclient.h>
#include "cJSON.h"

#include "boards_common.h"
#include "system_manager.h"
#include "env_module.h"
#include "ota_module.h"

#if CONFIG_LCD
#include "display_module.h"
#endif

#if CONFIG_XIAOZHI_ACT_V2
#include "hmac_license.h"
#endif

#define TAG "ota"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static struct webclient_session* session = NULL;
static bk_webclient_input_t input;
#if CONFIG_XIAOZHI_ACT_V2
static bk_webclient_input_t act_input;
#endif
static char active_code[10];
static char ota_ver[64];
#if CONFIG_XIAOZHI_OTA_EN
static char ota_url[128];
#endif
#if CONFIG_PROTOCOL_USE_WSS
static bool is_wss = false;
static char wss_url[128];
static char wss_token[64];
#endif
#if CONFIG_PROTOCOL_USE_MQTT
static bool is_mqtt = false;
static char mqtt_url[128];
static char mqtt_client_id[128];
static char mqtt_usr_name[64];
static char mqtt_pwd[64];
static char mqtt_pub_top[64];
#endif
static char challenge_str[64];
static beken_thread_t check_thr = NULL;
static beken_semaphore_t start_sem;
static bool is_active = false;

#if CONFIG_PROTOCOL_USE_MQTT
static void _ota_set_mqtt_url_from_endpoint(const char *endpoint)
{
    if (endpoint == NULL || endpoint[0] == '\0') {
        snprintf(mqtt_url, sizeof(mqtt_url), "%s", XIAOZHI_MQTT_URI);
        return;
    }

    if (strncmp(endpoint, "tcp://", 6) == 0 ||
        strncmp(endpoint, "ssl://", 6) == 0 ||
        strncmp(endpoint, "tls://", 6) == 0) {
        snprintf(mqtt_url, sizeof(mqtt_url), "%s", endpoint);
    } else if (strchr(endpoint, ':') != NULL) {
        snprintf(mqtt_url, sizeof(mqtt_url), "tcp://%s", endpoint);
    } else {
        snprintf(mqtt_url, sizeof(mqtt_url), "tcp://%s:2883", endpoint);
    }
}
#endif

static void _ota_notify_sys_actived(void)
{
    sysMsg_t msg = {0};

    msg.event = SYSTEM_EVENT_DEV_ACTIVE_DONE;
    system_manager_instance()->send_msg(&msg);
}

static int _ota_webclient_create(void)
{
    int ret = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(1024);
    if (session == NULL)
    {
        LOGE("session create failed.\n");
        ret = -1;
    }
    return ret;
}

static void _ota_webclient_close(void)
{
    if (session)
    {
        webclient_close(session);
    }
}

static void _ota_recv_json_handle(char* data)
{
    cJSON* root = cJSON_Parse(data);
    if(root == NULL){
        LOGE("json parese failed\r\n");
    }

#if CONFIG_XIAOZHI_OTA_EN
    cJSON* firmware = cJSON_GetObjectItem(root, "firmware");
    if(firmware == NULL){
        LOGE("firmware find err\r\n");
    }
    else{
        cJSON* version = cJSON_GetObjectItem(firmware, "version");
        if(version == NULL){
            LOGE("firmware ver find err\r\n");
        }
        else{
            sprintf(ota_ver, "%s", cJSON_GetObjectItem(firmware, "version")->valuestring);
            LOGI("vers: %s \r\n", ota_ver);
        }
        cJSON* url = cJSON_GetObjectItem(firmware, "url");
        if(url == NULL){
            LOGE("url find err\r\n");
        }
        else{
            sprintf(ota_url, "%s", cJSON_GetObjectItem(firmware, "url")->valuestring);
            LOGI("url: %s \r\n", ota_url);
        }
    }
#endif

#if CONFIG_PROTOCOL_USE_WSS
    cJSON* websocket = cJSON_GetObjectItem(root, "websocket");
    if(websocket == NULL){
        LOGE("websocket info get failed\r\n");
    }
    else{
        cJSON* url = cJSON_GetObjectItem(websocket, "url");
        if(url == NULL){
            LOGE("url find err\r\n");
        }
        else{
            sprintf(wss_url, "%s", cJSON_GetObjectItem(websocket, "url")->valuestring);
            cJSON* token = cJSON_GetObjectItem(websocket, "token");
            if(token == NULL){
                LOGE("token find err\r\n");
            }
            else{
                sprintf(wss_token, "%s", cJSON_GetObjectItem(websocket, "token")->valuestring);
                is_wss = true;
            }
        }
    }
#endif

#if CONFIG_PROTOCOL_USE_MQTT
    cJSON* mqtt = cJSON_GetObjectItem(root, "mqtt");
    if(mqtt == NULL){
        LOGE("mqtt info get failed\r\n");
        goto _out;
    }

    cJSON* endpoint = cJSON_GetObjectItem(mqtt, "endpoint");
    if(endpoint == NULL){
        LOGE("endpoint find err\r\n");
        goto _out;
    }
    else{
        _ota_set_mqtt_url_from_endpoint(cJSON_GetObjectItem(mqtt, "endpoint")->valuestring);
    }

    cJSON* client_id = cJSON_GetObjectItem(mqtt, "client_id");
    if(client_id == NULL){
        LOGE("client id find err\r\n");
        goto _out;
    }
    else{
        sprintf(mqtt_client_id, "%s", cJSON_GetObjectItem(mqtt, "client_id")->valuestring);
    }

    cJSON* username = cJSON_GetObjectItem(mqtt, "username");
    if(username == NULL){
        LOGE("username find err\r\n");
        goto _out;
    }
    else{
        sprintf(mqtt_usr_name, "%s", cJSON_GetObjectItem(mqtt, "username")->valuestring);
    }

    cJSON* password = cJSON_GetObjectItem(mqtt, "password");
    if(password == NULL){
        LOGE("password find err\r\n");
        goto _out;
    }
    else{
        sprintf(mqtt_pwd, "%s", cJSON_GetObjectItem(mqtt, "password")->valuestring);
    }

    cJSON* publish_topic = cJSON_GetObjectItem(mqtt, "publish_topic");
    if(publish_topic == NULL){
        LOGE("publish_topic find err\r\n");
        goto _out;
    }
    else{
        is_mqtt= true;
        sprintf(mqtt_pub_top, "%s", cJSON_GetObjectItem(mqtt, "publish_topic")->valuestring);
    }
#endif

    cJSON* activation = cJSON_GetObjectItem(root, "activation");
    if(activation == NULL){
        is_active = true;
        LOGE("activation find err\r\n");
        goto _out;
    }

    cJSON* code = cJSON_GetObjectItem(activation, "code");
    if(code == NULL){
        LOGE("active code find err\r\n");
        goto _out;
    }
    os_memcpy(active_code, cJSON_GetObjectItem(activation, "code")->valuestring, 10);
    LOGI("active code:%s \r\n", active_code);
    
    cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
    if(challenge == NULL){
        LOGE("challenge find err\r\n");
        goto _out;
    }
    
    os_memcpy(challenge_str, cJSON_GetObjectItem(activation, "challenge")->valuestring, 64);
    LOGI("challenge_str: %s \r\n", challenge_str);
    
_out:
    cJSON_Delete(root);
}

static void _ota_set_check_version_url(char* url)
{
    input.url = url;
}

static void _ota_set_post_data(char* data)
{
    input.post_data = data;
}

static int _ota_check_version_(void)
{
    /*send https post here*/
    int ret = 0;
    input.rx_buffer_size = 4096;
	int rx_buffer_size = input.rx_buffer_size;
	char *uri = os_strdup(input.url);
	char *post_data = NULL;
	size_t data_len = 0;
	unsigned char *buffer = NULL;
	int bytes_read, resp_status;
    char* mac = board_instance()->getMac();
    char* uid = board_instance()->getUid();
    LOGI("mac: %s\r uid: %s\r\n", mac, uid);

	if (input.post_data) {
		post_data = os_strdup(input.post_data);
		data_len = strlen(post_data);
	}
    
	char *rep_data = ( char *) os_zalloc(2*1024);
	if (rep_data == NULL)
	{
		LOGE("no memory for receive response buffer.\n");
		ret = -5;
		goto __exit;
	}
    
    LOGI("post_data: %s\r\n", post_data);
	buffer = (unsigned char *) web_malloc(rx_buffer_size);
	if (buffer == NULL)
	{
		LOGE("no memory for receive response buffer.\n");
		
	}
    
	/* build header for upload */
    if(_ota_webclient_create()){
        LOGE("post webclient create failed\r\n");
        ret = -1;
		goto __exit;
    }
    
    LOGI("Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session,"Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session,"Host: %s\r\n", XIAOZHI_BACKEND_HTTP_HOST);
#if !CONFIG_XIAOZHI_ACT_V2
    webclient_header_fields_add(session,"Activation-Version:1\r\n");
#else
    webclient_header_fields_add(session,"Activation-Version:2\r\n");
    webclient_header_fields_add(session,"Serial-Number:%s\r\n", hmac_get_serial_number());
#endif
    //webclient_header_fields_add(session,"Device-Id: c8:47:8c:59:ce:7e\r\n");
    //webclient_header_fields_add(session,"Client-Id: e6a322d5-99af-4af1-b398-c8478c59ce7e\r\n");
    webclient_header_fields_add(session,"User-Agent: %s/%s\r\n", USER_AGENT_NAME, USER_AGENT_VER);
    webclient_header_fields_add(session,"Device-Id: %s\r\n", mac);
    webclient_header_fields_add(session,"Client-Id: %s\r\n", uid);
    webclient_header_fields_add(session,"Content-Type: application/json\r\n");
    
	/* send POST request by default header */
    resp_status = webclient_post(session, uri, post_data, data_len);
	if (post_data && (resp_status != 200))
	{
		LOGE("webclient POST request failed, response(%d) error.\n", resp_status);
		ret = -1;
		goto __exit;
	}
    LOGI("resp_status: %d \r\n", resp_status);

	do
	{
		bytes_read = webclient_read(session, buffer, rx_buffer_size);
		if (bytes_read <= 0)
		{
			break;
		}
		strncat(rep_data,(char*)buffer,bytes_read);
	} while (1);
    
	LOGI("rep data: %s.\n", rep_data);
    bk_printf("rep data: %s.\n", rep_data);
    _ota_recv_json_handle(rep_data);

__exit:
    if (session)
    {
        webclient_close(session);
    }

	if (buffer)
	{
		web_free(buffer);
	}

	if (uri)
	{
		web_free(uri);
	}
    
	if (post_data)
	{
		web_free(post_data);
	}
    
	if (rep_data)
	{
		web_free(rep_data);
	}

	return ret;
}

#if CONFIG_XIAOZHI_ACT_V2
static void _ota_activation_data_set(char* challenge)
{
    LOGI("set activation data\r\n");
    act_input.url = "http://" XIAOZHI_BACKEND_HTTP_HOST "/xiaozhi/ota/activate";

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "algorithm", "hmac-sha256");
    cJSON_AddStringToObject(payload, "serial_number", (char*)hmac_get_serial_number());
    cJSON_AddStringToObject(payload, "challenge", challenge);
    cJSON_AddStringToObject(payload, "hmac", (char*)hmac_get_calculate_by_challenge(challenge));
    char* json_str = cJSON_PrintUnformatted(payload);
    act_input.post_data = json_str;
    LOGI("Activation payload: %s\r\n", act_input.post_data);
    cJSON_Delete(payload);
}

static int _ota_hmac_active_challenge(void)
{
    /*send https post here*/
    int ret = 0;
    act_input.rx_buffer_size = 4096;
    int rx_buffer_size = act_input.rx_buffer_size;
    char *uri = os_strdup(act_input.url);
    char *post_data = NULL;
    size_t data_len = 0;
    unsigned char *buffer = NULL;
    //size_t buffer_len = 0;
    int bytes_read, resp_status;
    char* mac = board_instance()->getMac();
    char* uid = board_instance()->getUid();
    LOGI("uri: %s\r\n", uri);

    if (act_input.post_data) 
    {
        post_data = os_strdup(act_input.post_data);
        data_len = strlen(post_data);
    }
    
    char *rep_data = ( char *) os_zalloc(2*1024);
    if (rep_data == NULL)
    {
        LOGE("no memory for receive response buffer.\n");
        ret = -5;
        goto __exit;
    }

    buffer = (unsigned char *) web_malloc(rx_buffer_size);
    if (buffer == NULL)
    {
        LOGE("no memory for receive response buffer.\n");
    }
    
    /* build header for upload */
    if(_ota_webclient_create())
    {
        LOGE("post webclient create failed\r\n");
        ret = -1;
        goto __exit;
    }
    
    LOGI("Content-Length: %d\rdata: %s\r\n", strlen(post_data), post_data);
    webclient_header_fields_add(session,"Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session,"Host: %s\r\n", XIAOZHI_BACKEND_HTTP_HOST);
    //webclient_header_fields_add(session,"Device-Id: c8:47:8c:59:ce:7e\r\n");
    //webclient_header_fields_add(session,"Client-Id: e6a322d5-99af-4af1-b398-c8478c59ce7e\r\n");
#if !CONFIG_XIAOZHI_ACT_V2
    webclient_header_fields_add(session,"Activation-Version:1\r\n");
#else
    webclient_header_fields_add(session,"Activation-Version:2\r\n");
    webclient_header_fields_add(session,"Serial-Number:%s\r\n", hmac_get_serial_number());
#endif
    webclient_header_fields_add(session,"User-Agent: %s/%s\r\n", USER_AGENT_NAME, USER_AGENT_VER);
    webclient_header_fields_add(session,"Device-Id: %s\r\n", mac);
    webclient_header_fields_add(session,"Client-Id: %s\r\n", uid);
    webclient_header_fields_add(session,"Content-Type: application/json\r\n");

#if 0
    /* send POST request by default header */
    resp_status = webclient_request(CONFIG_ACTIVATION_URL, session->header->buffer, post_data, data_len, (void**)&buffer, &buffer_len);
    LOGI("resp_status: %d \r\n", resp_status);

    rtos_delay_milliseconds(50);
    LOGI("rep data: %s.\n", rep_data);
#else
    /* send POST request by default header */
    resp_status = webclient_post(session, uri, post_data, data_len);
    if (post_data && (resp_status != 200))
    {
        LOGE("webclient POST request failed, response(%d) error.\n", resp_status);
        ret = -1;
        goto __exit;
    }
    LOGI("resp_status: %d \r\n", resp_status);

    for(int i = 0; i < 100; i++)
    {
        bytes_read = webclient_read(session, buffer, rx_buffer_size);
        if (bytes_read <= 0)
        {
            continue;
        }
        strncat(rep_data,(char*)buffer,bytes_read);
        rtos_delay_milliseconds(20);
        LOGI("rep data: %s.\n", rep_data);
    }
    
    //_ota_recv_json_handle(rep_data);
#endif

__exit:
    if (session)
    {
        webclient_close(session);
    }

    if (buffer)
    {
        web_free(buffer);
    }

    if (uri)
    {
        web_free(uri);
    }
    
    if (post_data)
    {
        web_free(post_data);
    }
    
    if (rep_data)
    {
        web_free(rep_data);
    }

    return ret;
}
#endif

#if CONFIG_PROTOCOL_USE_WSS
static char* _ota_get_wss_url(void)
{
    if(is_wss){
        return wss_url;
    }
    else{
        return NULL;
    }
}

static char* _ota_get_wss_token(void)
{
    if(is_wss){
        return wss_token;
    }
    else{
        return NULL;
    }
}
#endif

#if CONFIG_PROTOCOL_USE_MQTT
static char* _ota_get_mqtt_url(void)
{
    if(is_mqtt){
        return mqtt_url;
    }
    else{
        return NULL;
    }
}
static char* _ota_get_mqtt_client_id(void)
{
    if(is_mqtt){
        return mqtt_client_id;
    }
    else{
        return NULL;
    }
}

static char* _ota_get_mqtt_user_name(void)
{
    if(is_mqtt){
        return mqtt_usr_name;
    }
    else{
        return NULL;
    }
}

static char* _ota_get_mqtt_password(void)
{
    if(is_mqtt){
        return mqtt_pwd;
    }
    else{
        return NULL;
    }
}

static char* _ota_get_mqtt_user_public_topic(void)
{
    if(is_mqtt){
        return mqtt_pub_top;
    }
    else{
        return NULL;
    }
}
#endif

static int _ota_parse_version(char *version, int *ver_num, int size)
{
    char *token = NULL;
    char *ptr = NULL;
    int count = 0;

    if (version == NULL || ver_num == NULL || size <= 0) {
        LOGE("invalid param\r\n");
        return -1;
    }

    token = strtok_r(version, ".", &ptr);

    while (token) {
        ver_num[count] = atoi(token);
        count++;
        if (count >= size) {
            break;
        }
        token = strtok_r(NULL, ".", &ptr);
    }

    return count;
}

static bool _ota_has_new_version(void)
{
    int cur_ver[8] = {0};
    int new_ver[8] = {0};
    int cur_count = 0;
    int new_count = 0;

    LOGI("cur_ver: %s, new_ver: %s\r\n", USER_AGENT_VER, ota_ver);

    cur_count = _ota_parse_version(USER_AGENT_VER, cur_ver, 8);
    new_count = _ota_parse_version(ota_ver, new_ver, 8);
    if (cur_count <= 0 || new_count <= 0) {
        LOGE("parse version err\r\n");
        return false;
    }

    int min_count = cur_count < new_count ? cur_count : new_count;
    for (int i = 0; i < min_count; i++) {
        if (new_ver[i] > cur_ver[i]) {
            return true;
        } else if (new_ver[i] < cur_ver[i]) {
            return false;
        }
    }
    
    return (new_count > cur_count);
}

static bool _ota_has_activation_code(void)
{
    LOGI("is_active: %d\r\n", is_active);
    return !is_active;
}

static char* _ota_get_act_code(void)
{
    return active_code;
}

static bool _ota_is_dev_active(void)
{
    return is_active;
}

#if CONFIG_LCD
static void _ota_disp_act_code(char* code)
{
    char *str = (char*)os_malloc(sizeof(char)*64);
    sysMsg_t msg = {0};
    msg.event = SYSTEM_EVENT_DEV_ACTIVE_CODE_DISP;
    snprintf(str, 64, "%s: %s", DISP_ACTIVE_CODE_TEXT, code);
    msg.param = (void*)str;
    system_manager_instance()->send_msg(&msg);
}
#endif

static void _ota_notify_sys_active_start(void)
{
    static bool is_notify = false;
    char *code = _ota_get_act_code();

    if (is_notify) {
        return;
    }
    is_notify = true;

    LOGI("------active_code-------\r\n");
    LOGI("------%s-------\r\n", code);

#if CONFIG_LCD
    _ota_disp_act_code(code);
#endif
    
    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_DEV_ACTIVE_START);
}

#if CONFIG_XIAOZHI_OTA_EN
static int _ota_application_update(char* url)
{
    int ret = 0;

    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_OTA_START);
    ret = bk_http_ota_download(url);
    if(BK_OK != ret){
        LOGE("ota download bin err\r\n");
    }
    return ret;
}

#endif

static void _ota_check_thread(beken_thread_arg_t arg)
{
    rtos_get_semaphore(&start_sem, BEKEN_WAIT_FOREVER);

#if 0
    wifi_link_status_t link_status;
    do{
        bk_wifi_sta_get_link_status(&link_status);
        rtos_delay_milliseconds(50);
    }while(link_status.state != WIFI_LINKSTATE_STA_CONNECTED);
#endif
    _ota_set_post_data(board_get_json_str());
    _ota_set_check_version_url(CONFIG_OTA_VERSION_URL);
#if CONFIG_XIAOZHI_ACT_V2
    _ota_activation_data_set(challenge_str);
#endif

    while(1){
        if (0 != _ota_check_version_()) {
            rtos_delay_milliseconds(300);
            continue;
        }
#if CONFIG_LCD
        int cnt = 0;
        while (!display_module_instance()->isDispReady()) {
            if (cnt++ > 1000) {
                LOGE("ui is not ready\r\n");
                cnt = 0;
            }
            rtos_delay_milliseconds(10);
        }
#endif

#if CONFIG_XIAOZHI_OTA_EN
        if(_ota_has_new_version()){
            /*ota update bin*/
            LOGI("start ota, url: %s\r\n", ota_url);
            _ota_application_update(ota_url);
        }
#endif

#if !CONFIG_XIAOZHI_ACT_V2
        if(_ota_has_activation_code()){
            _ota_notify_sys_active_start();
        }
        else{
            LOGI("dev is active\r\n");
            _ota_notify_sys_actived();
            goto _out;
        }
#else
        if(_ota_has_activation_code()){
            _ota_notify_sys_active_start();
            
            for (int i = 0; i < 10; i++) {
                if (0 == _ota_hmac_active_challenge()) {
                    break;
                }
                rtos_delay_milliseconds(3000);
            }
        }
        else{
            LOGI("dev is active\r\n");
            _ota_notify_sys_actived();
            goto _out;
        }
#endif

        rtos_delay_milliseconds(300);
    }

_out:
    os_free(input.post_data);
    rtos_deinit_semaphore(&start_sem);
    rtos_delete_thread(&check_thr);
    check_thr = NULL;
    start_sem = NULL;
}

static uint8_t _ota_event_callback(evt_ota event_param)
{
    LOGI("ota event cb, event: %d\r\n", event_param);

    switch (event_param) {
    case EVT_OTA_START:
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_OTA_START);
        break;
    case EVT_OTA_FAIL:
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_OTA_FAIL);
        break;
    default:
        break;
    }

    return BK_OK;
}

static int _ota_init(void)
{
    int ret = 0;

    ret = ota_event_callback_register(_ota_event_callback);
    if (ret != BK_OK) {
        LOGE("ota_event_callback_register fail, ret: %d\r\n", ret);
    }

    return ret;
}

static int _ota_deinit(void)
{
    return 0;
}

static int _ota_start(void)
{
    int ret = 0;

    ret = rtos_init_semaphore_ex(&start_sem, 1, 0);
    if(ret != 0){
        LOGE("err init sem\r\n");
        goto _out;
    }
    
    ret = rtos_create_thread(&check_thr, 7, "ota_thread"
            , _ota_check_thread, 4096, NULL);
    if(ret != 0){
        LOGE("err thread \r\n");
        goto _out;
    }
    
    rtos_set_semaphore(&start_sem);
_out:
    return ret;

}

static int _ota_stop(void)
{
    if(check_thr != NULL){
        rtos_delete_thread(&check_thr);
        check_thr = NULL;
    }
    if(start_sem != NULL){
        rtos_deinit_semaphore(&start_sem);
        start_sem  = NULL;
    }
    
    return 0;
}

static ota_module_t g_ota_manger =
{
    .super.init = _ota_init,
    .super.start = _ota_start,
    .super.stop = _ota_stop,
    .super.deinit = _ota_deinit,
    .isActive = _ota_is_dev_active,
#if CONFIG_PROTOCOL_USE_WSS
    .getUrl = _ota_get_wss_url,
    .getToken = _ota_get_wss_token,
#endif
#if CONFIG_PROTOCOL_USE_MQTT
    .getUrl = _ota_get_mqtt_url,
    .getClientId = _ota_get_mqtt_client_id,
    .getUsername = _ota_get_mqtt_user_name,
    .getPassword = _ota_get_mqtt_password,
    .getPubTopic = _ota_get_mqtt_user_public_topic,
#endif
};

ota_module_t *ota_instance(void)
{
    return &g_ota_manger;
}
