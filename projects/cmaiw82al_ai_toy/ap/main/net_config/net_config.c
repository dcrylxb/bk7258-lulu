#include "net_config.h"
#include "system_manager.h"
#include "env_module.h"
#include "boards_common.h"
#include "device_auth.h"

#if CONFIG_BK_NETWORK_PROVISIONING
#include "bk_network_provisioning.h"
#include <components/bluetooth/bk_dm_bluetooth.h>
#endif

#define TAG "STA"

#define BOARDING_OP_STATION_START          1
#define BOARDING_OP_AGENT_RSP              12
#define BOARDING_OP_SET_AGENT_INFO         13
#define BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME 15
#define BOARDING_OP_START_WIFI_SCAN        24
#define BOARDING_OP_BLE_DISABLE            10
#define BOARDING_OP_AUTH_SIGN              151
#define BOARDING_OP_SYNC_SUPPORTED_ENGINE  25
#define BOARDING_OP_SYNC_SUPPORTED_NETWORK 26
#define BOARDING_OP_SERVER_CHECK_VERSION   500

#define BLE_WIFI_SCAN_PAYLOAD_MAX          200
#define BLE_WIFI_SCAN_RESULT_INITIAL_DELAY_MS 200
#define BLE_WIFI_SCAN_RESULT_POLL_MS       300
#define BLE_WIFI_SCAN_RESULT_POLL_RETRIES  20
#define BLE_WIFI_SCAN_RESULT_FORCE_POLL_ATTEMPT 10
#define BLE_WIFI_SCAN_RESULT_TASK_STACK    4096
#define BLE_AGENT_PAYLOAD_MAX              512
#define BLE_AUTH_PAYLOAD_MAX               768
#define BLE_PAYLOAD_LOG_MAX                96

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static uint8 g_wifi_error_send_once_flag = 0;
static bool s_ble_split_scan_results = false;
static volatile bool s_ble_scan_result_sent = false;
static volatile bool s_ble_scan_done_event_seen = false;
static volatile bool s_ble_scan_result_task_running = false;
static volatile bool s_ble_station_start_pending = false;
static volatile uint32_t s_ble_scan_request_id = 0;
static char s_ble_agent_payload[BLE_AGENT_PAYLOAD_MAX];
static uint16_t s_ble_agent_payload_len = 0;

static int net_connect_ap(char *ssid, char *password, bool is_config);
static int net_start_ble_provisioning(void);

#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
static void net_config_ble_msg_handler(ble_prov_msg_t *msg);
static void net_config_provisioning_status_cb(bk_network_provisioning_status_t status, void *user_data);
static bk_err_t ble_send_wifi_scan_results_if_needed(uint32_t request_id, const char *source);
static uint32_t ble_start_wifi_scan_result_task(void);
static void ble_notify_supported_engine(const char *source);
static void ble_notify_supported_network(const char *source);
static void ble_notify_capabilities(const char *source);
static void ble_notify_agent_status(const char *source);
static void ble_remember_agent_payload(ble_prov_msg_t *msg, const char *source);
static uint16_t ble_build_agent_info_payload(char *payload, uint16_t max_len);
static void ble_log_payload_summary(const char *label, const char *payload, uint16_t length);
static void ble_parse_agent_response(const char *payload, uint16_t length);
static void ble_handle_auth_sign(ble_prov_msg_t *msg);
static void ble_notify_with_data(uint16_t opcode, int status, char *payload, uint16_t length, const char *source);
static void ble_notify_status(uint16_t opcode, int status, const char *source);
extern void bk_ble_np_init(void);

static void net_config_prepare_ble_provisioning(bool start_default)
{
    net_config_t *net_module = net_config_instance();

    if (!net_module->m_ble_provisioning_ready) {
        bk_register_network_provisioning_status_cb(net_config_provisioning_status_cb);
        bk_ble_provisioning_set_msg_handle_cb(net_config_ble_msg_handler);
        net_module->m_ble_provisioning_ready = true;
        LOGI("BLE provisioning callbacks registered\r\n");
    }

    if (start_default) {
        bk_network_provisioning_init(BK_NETWORK_PROVISIONING_TYPE_BLE);
        LOGI("BLE provisioning initialized; running state waits for status callback\r\n");
    }
}

static int ble_wlan_scan_done_handler(void *arg, event_module_t event_module,
                                      int event_id, void *event_data)
{
    s_ble_scan_done_event_seen = true;
    LOGI("BLE scan done event req:%lu module:%d event:%d task:%d\r\n",
         s_ble_scan_request_id, event_module, event_id, s_ble_scan_result_task_running);
    if (!s_ble_scan_result_task_running) {
        return ble_send_wifi_scan_results_if_needed(s_ble_scan_request_id, "event");
    }
    return BK_OK;
}

static bk_err_t ble_send_wifi_scan_results_if_needed(uint32_t request_id, const char *source)
{
    wifi_scan_result_t scan_result = {0};
    char payload[BLE_WIFI_SCAN_PAYLOAD_MAX];
    uint16_t len = 0;
    int i = 0;
    int next_index = 0;
    bk_err_t ret = BK_OK;

    if ((request_id != s_ble_scan_request_id) || s_ble_scan_result_sent) {
        LOGI("BLE provisioning scan result skipped source:%s req:%lu current:%lu sent:%d\r\n",
             source, request_id, s_ble_scan_request_id, s_ble_scan_result_sent);
        return BK_OK;
    }

    ret = bk_wifi_scan_get_result(&scan_result);
    if (ret != BK_OK) {
        LOGW("BLE provisioning scan result get failed source:%s req:%lu ret:%d\r\n",
             source, request_id, ret);
        return ret;
    }
    LOGI("BLE provisioning scan get_result source:%s req:%lu ret:%d ap_num:%d aps:%p\r\n",
         source, request_id, ret, scan_result.ap_num, scan_result.aps);

    s_ble_scan_result_sent = true;
    bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, ble_wlan_scan_done_handler);
    if (scan_result.ap_num == 0) {
        os_memset(payload, 0, sizeof(payload));
        len = os_snprintf(payload, sizeof(payload), "[]");
        LOGI("BLE provisioning scan result sent source:%s req:%lu total:0 payload:%s\r\n",
             source, request_id, payload);
        bk_ble_provisioning_event_notify_with_data(BOARDING_OP_START_WIFI_SCAN, 0, payload, len);
        goto exit;
    }

again:
    os_memset(payload, 0, sizeof(payload));
    len = os_snprintf(payload, sizeof(payload), "[");
    int packet_start_index = next_index;
    for (i = next_index; i < scan_result.ap_num; i++) {
        if (!os_strlen(scan_result.aps[i].ssid)) {
            continue;
        }
        if ((len + 5 + os_strlen(scan_result.aps[i].ssid)) > sizeof(payload)) {
            next_index = i;
            break;
        }
        if ((i != 0) && (len != 1)) {
            len += os_snprintf(payload + len, sizeof(payload) - len, ",");
        }
        len += os_snprintf(payload + len, sizeof(payload) - len, "\"%s\"", scan_result.aps[i].ssid);
        next_index = i + 1;
    }
    if (next_index <= packet_start_index) {
        next_index = scan_result.ap_num;
    }
    len += os_snprintf(payload + len, sizeof(payload) - len, "]");
    LOGI("BLE provisioning scan result sent source:%s req:%lu sent:%d total:%d payload:%s\r\n",
         source, request_id, next_index, scan_result.ap_num, payload);
    if ((next_index >= scan_result.ap_num) || !s_ble_split_scan_results) {
        bk_ble_provisioning_event_notify_with_data(BOARDING_OP_START_WIFI_SCAN, 0, payload, len);
    } else {
        bk_ble_provisioning_event_notify_with_data(BOARDING_OP_START_WIFI_SCAN, 1, payload, len);
        goto again;
    }

exit:
    bk_wifi_scan_free_result(&scan_result);
    return BK_OK;
}

static void ble_wifi_scan_result_task(beken_thread_arg_t arg)
{
    uint32_t request_id = (uint32_t)(uintptr_t)arg;

    LOGI("BLE scan result task start req:%lu\r\n", request_id);
    rtos_delay_milliseconds(BLE_WIFI_SCAN_RESULT_INITIAL_DELAY_MS);
    for (int retry = 0; retry < BLE_WIFI_SCAN_RESULT_POLL_RETRIES; retry++) {
        if ((request_id != s_ble_scan_request_id) || s_ble_scan_result_sent) {
            break;
        }

        LOGI("BLE scan poll req:%lu attempt:%d done:%d sent:%d\r\n",
             request_id, retry + 1, s_ble_scan_done_event_seen, s_ble_scan_result_sent);
        if (s_ble_scan_done_event_seen || (retry >= BLE_WIFI_SCAN_RESULT_FORCE_POLL_ATTEMPT)) {
            if (ble_send_wifi_scan_results_if_needed(request_id, "worker") == BK_OK &&
                s_ble_scan_result_sent) {
                break;
            }
        }
        rtos_delay_milliseconds(BLE_WIFI_SCAN_RESULT_POLL_MS);
    }

    if (request_id == s_ble_scan_request_id) {
        s_ble_scan_result_task_running = false;
    }
    LOGI("BLE scan result task exit req:%lu sent:%d\r\n", request_id, s_ble_scan_result_sent);
    rtos_delete_thread(NULL);
}

static uint32_t ble_start_wifi_scan_result_task(void)
{
    bk_err_t ret = BK_OK;
    uint32_t request_id = ++s_ble_scan_request_id;

    s_ble_scan_result_sent = false;
    s_ble_scan_done_event_seen = false;
    s_ble_scan_result_task_running = true;
    ret = rtos_create_thread(NULL,
                             7,
                             "ble_scan_rst",
                             (beken_thread_function_t)ble_wifi_scan_result_task,
                             BLE_WIFI_SCAN_RESULT_TASK_STACK,
                             (beken_thread_arg_t)(uintptr_t)request_id);
    if (ret != BK_OK) {
        s_ble_scan_result_task_running = false;
    }
    LOGI("BLE provisioning scan task create req:%lu ret:%d\r\n", request_id, ret);

    return request_id;
}

static void ble_abort_wifi_scan_request(uint32_t request_id)
{
    if (request_id == s_ble_scan_request_id) {
        s_ble_scan_result_sent = true;
        s_ble_scan_done_event_seen = false;
    }
    bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, ble_wlan_scan_done_handler);
}

static void ble_notify_wifi_scan_start_failed(uint32_t request_id, bk_err_t ret)
{
    if (ret == BK_OK) {
        return;
    }

    ble_abort_wifi_scan_request(request_id);
    LOGW("BLE provisioning scan start failed req:%lu ret:%d\r\n",
         request_id, ret);
    ble_notify_status(BOARDING_OP_START_WIFI_SCAN, BK_FAIL, "scan_start_failed");
}

static void ble_log_payload_summary(const char *label, const char *payload, uint16_t length)
{
    char summary[BLE_PAYLOAD_LOG_MAX + 1];
    uint16_t copy_len = length;

    if ((payload == NULL) || (length == 0)) {
        LOGI("%s payload empty len:%u\r\n", label, length);
        return;
    }

    if (copy_len > BLE_PAYLOAD_LOG_MAX) {
        copy_len = BLE_PAYLOAD_LOG_MAX;
    }
    os_memset(summary, 0, sizeof(summary));
    os_memcpy(summary, payload, copy_len);
    for (uint16_t i = 0; i < copy_len; i++) {
        if ((summary[i] < 0x20) || (summary[i] > 0x7e)) {
            summary[i] = '.';
        }
    }
    LOGI("%s payload len:%u preview:%s%s\r\n",
         label, length, summary, (length > copy_len) ? "..." : "");
}

static void ble_notify_status(uint16_t opcode, int status, const char *source)
{
    LOGI("BLE provisioning notify source:%s opcode:%u status:%d len:0\r\n",
         source, opcode, status);
    bk_ble_provisioning_event_notify(opcode, status);
}

static void ble_notify_with_data(uint16_t opcode, int status, char *payload, uint16_t length, const char *source)
{
    LOGI("BLE provisioning notify source:%s opcode:%u status:%d len:%u\r\n",
         source, opcode, status, length);
    ble_log_payload_summary("BLE provisioning notify", payload, length);
    bk_ble_provisioning_event_notify_with_data(opcode, status, payload, length);
}

static void ble_notify_supported_network(const char *source)
{
    uint8_t len = 0;
    uint8_t fallback_wifi = 0;
    uint8_t *val = bk_sconf_get_supported_network(&len);

    if ((val == NULL) || (len == 0)) {
        LOGW("BLE provisioning supported network empty source:%s, fallback wifi only\r\n", source);
        ble_notify_with_data(BOARDING_OP_SYNC_SUPPORTED_NETWORK,
                             0,
                             (char *)&fallback_wifi,
                             sizeof(fallback_wifi),
                             source);
        if (val != NULL) {
            os_free(val);
        }
        return;
    }

    LOGI("BLE provisioning supported network sent len:%u first:%u source:%s\r\n",
         len, val[0], source);
    ble_notify_with_data(BOARDING_OP_SYNC_SUPPORTED_NETWORK,
                         0,
                         (char *)val,
                         len,
                         source);
    os_free(val);
}

static void ble_notify_supported_engine(const char *source)
{
    uint8_t engine = 0;

    LOGI("BLE provisioning supported engine sent first:%u source:%s\r\n", engine, source);
    ble_notify_with_data(BOARDING_OP_SYNC_SUPPORTED_ENGINE,
                         BK_OK,
                         (char *)&engine,
                         sizeof(engine),
                         source);
}

static void ble_notify_capabilities(const char *source)
{
    LOGI("BLE provisioning capability notify source:%s\r\n", source);
    ble_notify_supported_engine(source);
    ble_notify_supported_network(source);
}

static void ble_remember_agent_payload(ble_prov_msg_t *msg, const char *source)
{
    uint16_t copy_len = 0;

    if ((msg == NULL) || (msg->param == 0) || (msg->length == 0)) {
        s_ble_agent_payload_len = 0;
        s_ble_agent_payload[0] = '\0';
        LOGI("BLE provisioning agent payload empty source:%s\r\n", source);
        return;
    }

    copy_len = msg->length;
    if (copy_len >= sizeof(s_ble_agent_payload)) {
        copy_len = sizeof(s_ble_agent_payload) - 1;
    }

    os_memset(s_ble_agent_payload, 0, sizeof(s_ble_agent_payload));
    os_memcpy(s_ble_agent_payload, (void *)(uintptr_t)msg->param, copy_len);
    s_ble_agent_payload_len = copy_len;
    LOGI("BLE provisioning agent payload stored source:%s len:%u total:%u\r\n",
         source, copy_len, msg->length);
    ble_log_payload_summary("BLE provisioning agent rx", s_ble_agent_payload, s_ble_agent_payload_len);
}

static uint16_t ble_build_agent_info_payload(char *payload, uint16_t max_len)
{
    char *device_id = NULL;

    if ((payload == NULL) || (max_len == 0)) {
        return 0;
    }

    device_id = board_instance()->getSubMac();
    if ((device_id == NULL) || (os_strlen(device_id) == 0)) {
        device_id = board_instance()->getUid();
    }

    os_memset(payload, 0, max_len);
    return os_snprintf(payload, max_len, "{\"channel\":\"%s\"}",
                       device_id ? device_id : "cmaiw82al_ai_toy");
}

static void ble_parse_agent_response(const char *payload, uint16_t length)
{
    cJSON *json = NULL;
    cJSON *channel_name = NULL;

    if ((payload == NULL) || (length == 0)) {
        LOGW("BLE provisioning agent response empty\r\n");
        return;
    }

    json = cJSON_Parse(payload);
    if (json == NULL) {
        LOGW("BLE provisioning agent response is not JSON\r\n");
        return;
    }

    channel_name = cJSON_GetObjectItemCaseSensitive(json, "channel_name");
    if (cJSON_IsString(channel_name) && (channel_name->valuestring != NULL)) {
        LOGI("BLE provisioning agent channel_name:%s\r\n", channel_name->valuestring);
    } else {
        LOGW("BLE provisioning agent response has no channel_name\r\n");
    }

    cJSON_Delete(json);
}

static void ble_notify_agent_status(const char *source)
{
    char payload[BLE_AGENT_PAYLOAD_MAX];
    uint16_t len;

    len = ble_build_agent_info_payload(payload, sizeof(payload));
    LOGI("BLE provisioning agent info notify source:%s len:%u\r\n", source, len);
    if (len > 0) {
        ble_notify_with_data(BOARDING_OP_SET_AGENT_INFO, BK_OK, payload, len, source);
    } else {
        ble_notify_status(BOARDING_OP_SET_AGENT_INFO, BK_FAIL, source);
    }
}

static void ble_handle_auth_sign(ble_prov_msg_t *msg)
{
    char payload[BLE_AUTH_PAYLOAD_MAX];
    char response[BLE_AUTH_PAYLOAD_MAX];
    char signature[DEVICE_AUTH_SIGNATURE_HEX_LEN + 1];
    cJSON *json = NULL;
    cJSON *signing_message = NULL;
    uint16_t copy_len = 0;
    uint16_t response_len = 0;

    if ((msg == NULL) || (msg->param == 0) || (msg->length == 0)) {
        LOGW("BLE auth.sign empty payload\r\n");
        ble_notify_status(BOARDING_OP_AUTH_SIGN, BK_FAIL, "auth_sign_empty");
        return;
    }

    copy_len = msg->length;
    if (copy_len >= sizeof(payload)) {
        copy_len = sizeof(payload) - 1;
    }
    os_memset(payload, 0, sizeof(payload));
    os_memcpy(payload, (void *)(uintptr_t)msg->param, copy_len);

    json = cJSON_ParseWithLength(payload, copy_len);
    if ((json != NULL) && cJSON_IsObject(json)) {
        signing_message = cJSON_GetObjectItemCaseSensitive(json, "signing_message");
        if (!cJSON_IsString(signing_message) || (signing_message->valuestring == NULL) ||
            (os_strlen(signing_message->valuestring) == 0)) {
            LOGW("BLE auth.sign missing signing_message\r\n");
            cJSON_Delete(json);
            ble_notify_status(BOARDING_OP_AUTH_SIGN, BK_FAIL, "auth_sign_no_message");
            return;
        }
    }

    if ((json != NULL) && cJSON_IsObject(json)) {
        if (device_auth_sign_message(signing_message->valuestring,
                                     signature,
                                     sizeof(signature)) != BK_OK) {
            LOGW("BLE auth.sign failed: device secret unavailable\r\n");
            cJSON_Delete(json);
            ble_notify_status(BOARDING_OP_AUTH_SIGN, BK_FAIL, "auth_sign_no_secret");
            return;
        }
    } else {
        if ((json != NULL) && !cJSON_IsObject(json)) {
            cJSON_Delete(json);
            json = NULL;
        }
        if (device_auth_sign_message(payload, signature, sizeof(signature)) != BK_OK) {
            LOGW("BLE auth.sign failed: device secret unavailable\r\n");
            ble_notify_status(BOARDING_OP_AUTH_SIGN, BK_FAIL, "auth_sign_no_secret");
            return;
        }
    }

    os_memset(response, 0, sizeof(response));
    response_len = os_snprintf(response, sizeof(response),
                               "{\"cmd\":\"auth.sign.result\",\"signature\":\"%s\"}",
                               signature);
    LOGI("BLE auth.sign response len:%u\r\n", response_len);
    ble_notify_with_data(BOARDING_OP_AUTH_SIGN, BK_OK, response, response_len, "auth_sign");
    if (json != NULL) {
        cJSON_Delete(json);
    }
}

static void net_config_ble_msg_handler(ble_prov_msg_t *msg)
{
    if (msg == NULL) {
        return;
    }

    LOGI("BLE provisioning msg event:%lu length:%u\r\n", msg->event, msg->length);
    ble_log_payload_summary("BLE provisioning msg", (char *)(uintptr_t)msg->param, msg->length);
    switch (msg->event) {
        case BOARDING_OP_STATION_START:
        {
            bk_ble_provisioning_info_t *bk_ble_provisioning_info = bk_ble_provisioning_get_boarding_info();

            if ((bk_ble_provisioning_info == NULL) ||
                (bk_ble_provisioning_info->ble_prov_info.ssid_value == NULL)) {
                LOGE("BLE provisioning station start without ssid\r\n");
                ble_notify_status(BOARDING_OP_STATION_START, BK_FAIL, "station_start_no_ssid");
                break;
            }

            net_config_instance()->connect_ap(bk_ble_provisioning_info->ble_prov_info.ssid_value,
                                              bk_ble_provisioning_info->ble_prov_info.password_value ?
                                              bk_ble_provisioning_info->ble_prov_info.password_value : "",
                                              true);
            s_ble_station_start_pending = true;
            bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, ble_wlan_scan_done_handler);
            break;
        }

        case BOARDING_OP_START_WIFI_SCAN:
        {
            bk_err_t unregister_ret;
            bk_err_t register_ret;
            bk_err_t ret;
            uint32_t request_id;

            LOGI("BOARDING_OP_START_WIFI_SCAN\r\n");
            if (msg->param) {
                s_ble_split_scan_results = (*(uint8_t *)msg->param == 1);
            } else {
                s_ble_split_scan_results = false;
            }
            unregister_ret = bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, ble_wlan_scan_done_handler);
            register_ret = bk_event_register_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, ble_wlan_scan_done_handler, NULL);
            LOGI("BLE provisioning scan register ret:%d unregister ret:%d\r\n",
                 register_ret, unregister_ret);
            request_id = ble_start_wifi_scan_result_task();
            ret = bk_wifi_scan_start(NULL);
            LOGI("BLE provisioning scan start req:%lu ret:%d split:%d\r\n",
                 request_id, ret, s_ble_split_scan_results);
            ble_notify_wifi_scan_start_failed(request_id, ret);
            break;
        }

        case BOARDING_OP_BLE_DISABLE:
            LOGI("BOARDING_OP_BLE_DISABLE\r\n");
            LOGI("BLE provisioning disable requested, close bluetooth\r\n");
            bk_bluetooth_deinit();
            net_config_instance()->m_ble_provisioning_active = false;
            break;

        case BOARDING_OP_AUTH_SIGN:
            LOGI("BOARDING_OP_AUTH_SIGN length:%u\r\n", msg->length);
            ble_handle_auth_sign(msg);
            break;

        case BOARDING_OP_AGENT_RSP:
            LOGI("BOARDING_OP_AGENT_RSP length:%u\r\n", msg->length);
            ble_remember_agent_payload(msg, "agent_rsp");
            ble_parse_agent_response(s_ble_agent_payload, s_ble_agent_payload_len);
            ble_notify_status(BOARDING_OP_AGENT_RSP, BK_OK, "agent_rsp_ack");
            break;

        case BOARDING_OP_SET_AGENT_INFO:
            LOGI("BOARDING_OP_SET_AGENT_INFO length:%u\r\n", msg->length);
            ble_remember_agent_payload(msg, "set_agent_info");
            ble_notify_agent_status("set_agent_info");
            break;

        case BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME:
            LOGI("BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME length:%u\r\n", msg->length);
            ble_notify_status(BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME, BK_OK, "first_time");
            break;

        case BOARDING_OP_SYNC_SUPPORTED_ENGINE:
            LOGI("BOARDING_OP_SYNC_SUPPORTED_ENGINE\r\n");
            ble_notify_capabilities("request_engine");
            break;

        case BOARDING_OP_SYNC_SUPPORTED_NETWORK:
            LOGI("BOARDING_OP_SYNC_SUPPORTED_NETWORK\r\n");
            ble_notify_capabilities("request_network");
            break;

        case BOARDING_OP_SERVER_CHECK_VERSION:
            LOGI("BOARDING_OP_SERVER_CHECK_VERSION\r\n");
            ble_notify_status(BOARDING_OP_SERVER_CHECK_VERSION, BK_OK, "version_check");
            LOGI("BLE provisioning active capability notify\r\n");
            ble_notify_capabilities("version_check");
            ble_notify_agent_status("version_check");
            break;

        default:
            LOGI("BLE provisioning msg ignored event:%lu\r\n", msg->event);
            break;
    }
}

static void net_config_provisioning_status_cb(bk_network_provisioning_status_t status, void *user_data)
{
    LOGI("BLE provisioning status:%d\r\n", status);
    switch (status) {
        case BK_NETWORK_PROVISIONING_STATUS_RUNNING:
            net_config_instance()->m_ble_provisioning_active = true;
            s_ble_station_start_pending = false;
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_NULL);
            break;

        case BK_NETWORK_PROVISIONING_STATUS_SUCCEED:
            if (bk_network_provisioning_get_type() == BK_NETWORK_PROVISIONING_TYPE_BLE) {
                netif_if_t netif_idx = (netif_if_t)(uintptr_t)user_data;
                netif_ip4_config_t ip4_config = {0};

                bk_netif_get_ip4_config(netif_idx, &ip4_config);
                LOGI("BLE provisioning succeed netif:%d ip:%s\r\n", netif_idx, ip4_config.ip);
                ble_notify_with_data(BOARDING_OP_STATION_START, BK_OK,
                                     ip4_config.ip, os_strlen(ip4_config.ip),
                                     "provisioning_success");
                ble_notify_capabilities("provisioning_success");
                ble_notify_agent_status("provisioning_success");
                system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONFIG_DONE);
            }
            net_config_instance()->m_ble_provisioning_active = false;
            s_ble_station_start_pending = false;
            break;

        case BK_NETWORK_PROVISIONING_STATUS_FAILED:
        case BK_NETWORK_PROVISIONING_STATUS_RECONNECT_FAILED:
            net_config_instance()->m_ble_provisioning_active = false;
            s_ble_station_start_pending = false;
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONNECT_FAIL);
            break;

        default:
            break;
    }
}
#endif
static int app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *sta_disconnected;
    wifi_event_sta_connected_t *sta_connected;

    switch (event_id) {
        case EVENT_WIFI_STA_CONNECTED:
            sta_connected = (wifi_event_sta_connected_t *)event_data;
            LOGI("STA connected to %s\n", sta_connected->ssid);
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
            sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            LOGI("STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
            if(WIFI_REASON_WRONG_PASSWORD == sta_disconnected->disconnect_reason)
            {
                if(g_wifi_error_send_once_flag)
                {
                    g_wifi_error_send_once_flag=0;
                    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_PASSWORD_ERR);
                }
            }
            else if(WIFI_REASON_NO_AP_FOUND == sta_disconnected->disconnect_reason)
            {
                if(g_wifi_error_send_once_flag)
                {
                    g_wifi_error_send_once_flag=0;
                    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_NO_AP);
                }
            }
            else
            {
                if(g_wifi_error_send_once_flag)
                {
                    g_wifi_error_send_once_flag=0;
                    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONNECT_FAIL);
                }
            }
            net_config_instance()->m_is_config = false;
            break;

        default:
            LOGI("rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}


static int app_netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;

    switch (event_id) {
        case EVENT_NETIF_GOT_IP4:
            got_ip = (netif_event_got_ip4_t *)event_data;
            LOGI("%s got ip\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif");
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
            if (s_ble_station_start_pending) {
                LOGI("BLE provisioning pending station got ip event ip:%s, notify station success\r\n", got_ip->ip);
                ble_notify_with_data(BOARDING_OP_STATION_START, BK_OK,
                                     got_ip->ip, os_strlen(got_ip->ip),
                                     "netif_got_ip");
                ble_notify_capabilities("netif_got_ip");
                ble_notify_agent_status("netif_got_ip");
                system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONFIG_DONE);
                s_ble_station_start_pending = false;
                net_config_instance()->m_ble_provisioning_active = false;
                net_config_instance()->m_is_config = false;
                break;
            }
#endif
            if(net_config_instance()->m_is_config)
            {
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
                if (net_config_instance()->m_ble_provisioning_active) {
                    LOGI("BLE provisioning got ip event ip:%s, notify station success\r\n", got_ip->ip);
                    ble_notify_with_data(BOARDING_OP_STATION_START, BK_OK,
                                         got_ip->ip, os_strlen(got_ip->ip),
                                         "netif_got_ip");
                    ble_notify_capabilities("netif_got_ip");
                    ble_notify_agent_status("netif_got_ip");
                    system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONFIG_DONE);
                    net_config_instance()->m_ble_provisioning_active = false;
                    break;
                }
#endif
                //通知ap配网模块完成，并退出
                extern int wifi_config_ok_notify(void);
                wifi_config_ok_notify();
                
                system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONFIG_DONE);
            }
            else
            {
                g_wifi_error_send_once_flag=1;
                system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONNECTED);
            }
            net_config_instance()->m_is_config = false;
            break;

        default:
            LOGI("rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}

static void wifi_event_handler_init(void)
{
	BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, app_wifi_event_cb, NULL));
	BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, app_netif_event_cb, NULL));
}
static void wifi_event_handler_deinit(void)
{
	BK_LOG_ON_ERR(bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, app_wifi_event_cb));
	BK_LOG_ON_ERR(bk_event_unregister_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, app_netif_event_cb));
}

static int net_config_set_info(net_info_t* info)
{
    if(info == NULL)
    {
        LOGI("set info NULL\n");
        return -1;
    }
    else
    {
        strncpy(net_config_instance()->m_net_info.ssid, info->ssid, WIFI_SSID_STR_LEN);
        strncpy(net_config_instance()->m_net_info.pwd, info->pwd, WIFI_PASSWORD_LEN);
    }

    return 0;
}

static int net_config_init(void)
{
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
    net_config_prepare_ble_provisioning(false);
#endif
    return 0;
}

static int net_config_start(void)
{
    net_config_t *net_module = net_config_instance();

    if(BK_OK == env_module_instance()->getNetInfo(&net_config_instance()->m_net_info))  //有配网信息
    {
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
        net_config_prepare_ble_provisioning(false);
#endif
        NET_LOGI("[%s][%d] ready connect wifi\r\n", __FUNCTION__, __LINE__);
        net_module->connect_ap(net_config_instance()->m_net_info.ssid, net_config_instance()->m_net_info.pwd, false);
    }
    else
    {
        NET_LOGI("[%s][%d] wifi info is NULL, start ap\r\n", __FUNCTION__, __LINE__);
        wifi_event_handler_deinit();
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_NULL);
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
        erase_network_auto_reconnect_info();
        net_config_prepare_ble_provisioning(true);
#else
        wificonfig_test();
#endif
    }

    return 0;
}

static int net_config_stop(void)
{
    return 0;
}

static int net_config_deinit(void)
{
    return 0;
}

static int net_connect_ap(char *ssid, char *password, bool is_config)
{
    net_config_t *net_module = net_config_instance();
    wifi_sta_config_t sta_config = WIFI_DEFAULT_STA_CONFIG();

    g_wifi_error_send_once_flag = 1;
    wifi_event_handler_deinit();
    wifi_event_handler_init();
    os_snprintf(sta_config.ssid, WIFI_SSID_STR_LEN, "%s", ssid);
    os_snprintf(sta_config.password, WIFI_PASSWORD_LEN, "%s", password);
    BK_LOG_ON_ERR(bk_wifi_sta_set_config(&sta_config));
    BK_LOG_ON_ERR(bk_wifi_sta_start());

    NET_LOGI("[%s][%d] connect %s %s, is_config:%d\r\n", __FUNCTION__, __LINE__, ssid, password, is_config);
    if(!is_config)
    {
        net_module->m_is_config = false;
        system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_NET_CONNECTING);
    }
    else
    {
        net_module->m_is_config = true;
    }

    return 0;
}

static int net_start_ble_provisioning(void)
{
#if CONFIG_BK_NETWORK_PROVISIONING && CONFIG_BK_BLE_PROVISIONING
    net_config_t *net_module = net_config_instance();

    if (!net_module->m_ble_provisioning_ready) {
        LOGW("BLE provisioning callbacks not ready, init now\r\n");
        net_config_prepare_ble_provisioning(false);
    }

    if (net_module->m_ble_provisioning_active) {
        LOGI("BLE network provisioning already active, restart BLE provisioning advertising\r\n");
        bk_ble_np_init();
        return BK_OK;
    }

    LOGI("start BLE network provisioning\r\n");
    bk_network_provisioning_start(BK_NETWORK_PROVISIONING_TYPE_BLE);
    return BK_OK;
#else
    LOGW("BLE network provisioning disabled by config\r\n");
    return BK_FAIL;
#endif
}


static net_config_t g_net_cfg = 
{
    .super.init = net_config_init,
    .super.start = net_config_start,
    .super.stop = net_config_stop,
    .super.deinit = net_config_deinit,
    .setNetInfo = net_config_set_info,
    .connect_ap = net_connect_ap,
    .start_ble_provisioning = net_start_ble_provisioning,
};

net_config_t* net_config_instance(void)
{
    return &g_net_cfg;
}
