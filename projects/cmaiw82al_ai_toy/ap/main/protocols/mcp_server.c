#include "mcp_server.h"
#include <os/mem.h>
#include <os/str.h>
#include <string.h>
#include "boards_common.h"
#include "protocol.h"
#include "protocol_websocket.h"
#include "iot_devices.h"

#define TAG "mcp_s"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static beken_thread_t _call_func_thr = NULL;
static beken_queue_t mcp_queue = NULL;
static char cap_url[192];
static char cap_token[128];

#define MCP_MANAGER_REQUEST_SLOTS 8

typedef struct {
    int mcp_id;
    char manager_request_id[64];
} mcp_manager_request_t;

static mcp_manager_request_t manager_requests[MCP_MANAGER_REQUEST_SLOTS];

#define MCP_VISION_UPLOAD_PATH "/xiaozhi/api/vision"

static bool mcp_server_vision_url_valid(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return false;
    }

    if (os_strncmp(url, "http://", 7) != 0 &&
        os_strncmp(url, "https://", 8) != 0) {
        return false;
    }

    if (os_strstr(url, "dashscope.aliyuncs.com") != NULL ||
        os_strstr(url, "/compatible-mode/") != NULL) {
        return false;
    }

    return os_strstr(url, MCP_VISION_UPLOAD_PATH) != NULL;
}

static void mcp_server_set_vision_fallback(void)
{
    os_snprintf(cap_url, sizeof(cap_url), "http://%s%s",
                XIAOZHI_BACKEND_HTTP_HOST, MCP_VISION_UPLOAD_PATH);
}

static void mcp_server_call_function_main(beken_thread_arg_t arg)
{
    int ret = 0;
    mcp_cf_t msg = {0};
    mcp_server_t *server = mcp_server_instance();
    property_list_t* tmp, *n;

    while(1)
    {
        ret = rtos_pop_from_queue(&mcp_queue, &msg, BEKEN_WAIT_FOREVER);
        if(ret != 0){
            LOGE("queue err\r\n");
        }
        LOGD("call func, id: %d, name: %s, args: %s\r\n", msg.id, msg.func_name, msg.arg_json);

        dl_list_for_each_safe(tmp, n, &(server->tools_head), property_list_t, m_list)
        {
            if(!strcmp(tmp->name, msg.func_name))
            {
                if(tmp->func_cb != NULL)
                {
                    tmp->func_cb(&msg);
                }
                break;
            }
        }
        cJSON_free(msg.arg_json);
        os_free(msg.func_name);
        
        rtos_delay_milliseconds(10);
    }
}

static void mcp_server_reply_tool_list(int id, char* result)
{
    char* payload = (char*)psram_malloc(10 * 1024 * sizeof(char));
    sprintf(payload, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);
    LOGD("json: %s\r\n", payload);
    protocol_instance()->sendMcpMessage((uint8_t*)payload);
    os_free(payload);
}

static void mcp_server_set_manager_request_id(int id, const char *request_id)
{
    int free_slot = -1;

    if (request_id == NULL || request_id[0] == '\0') {
        return;
    }

    for (int i = 0; i < MCP_MANAGER_REQUEST_SLOTS; i++) {
        if (manager_requests[i].mcp_id == id) {
            os_snprintf(manager_requests[i].manager_request_id,
                        sizeof(manager_requests[i].manager_request_id),
                        "%s",
                        request_id);
            return;
        }
        if (free_slot < 0 && manager_requests[i].mcp_id == 0) {
            free_slot = i;
        }
    }

    if (free_slot < 0) {
        free_slot = 0;
    }

    manager_requests[free_slot].mcp_id = id;
    os_snprintf(manager_requests[free_slot].manager_request_id,
                sizeof(manager_requests[free_slot].manager_request_id),
                "%s",
                request_id);
}

static bool mcp_server_take_manager_request_id(int id, char *request_id, size_t request_id_size)
{
    if (request_id == NULL || request_id_size == 0) {
        return false;
    }

    for (int i = 0; i < MCP_MANAGER_REQUEST_SLOTS; i++) {
        if (manager_requests[i].mcp_id == id && manager_requests[i].manager_request_id[0] != '\0') {
            os_snprintf(request_id, request_id_size, "%s", manager_requests[i].manager_request_id);
            manager_requests[i].mcp_id = 0;
            manager_requests[i].manager_request_id[0] = '\0';
            return true;
        }
    }

    return false;
}

static bool mcp_server_reply_manager_response(int id, int status, char *result, char *error_message)
{
    char request_id[64] = {0};
    cJSON *response = NULL;
    cJSON *body = NULL;
    char *response_str = NULL;
    bool sent = false;

    if (!mcp_server_take_manager_request_id(id, request_id, sizeof(request_id))) {
        return false;
    }

    response = cJSON_CreateObject();
    if (response == NULL) {
        return false;
    }

    cJSON_AddStringToObject(response, "id", request_id);
    cJSON_AddNumberToObject(response, "status", status);

    if (status >= 200 && status < 300) {
        if (result != NULL) {
            body = cJSON_Parse(result);
        }
        if (body == NULL) {
            body = cJSON_CreateObject();
        }
        if (body != NULL) {
            cJSON_AddItemToObject(response, "body", body);
        }
    } else {
        cJSON_AddStringToObject(response, "error", error_message);
    }

    response_str = cJSON_PrintUnformatted(response);
    if (response_str != NULL) {
        protocol_websocket_instance()->sendManagerResponse((uint8_t *)response_str);
        cJSON_free(response_str);
        sent = true;
    }

    cJSON_Delete(response);
    return sent;
}

static void mcp_server_reply_result(int id, char* result)
{
    char* payload = (char*)psram_malloc(1024 * sizeof(char));
    os_snprintf(payload, 1024 * sizeof(char), "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result);
    if (!mcp_server_reply_manager_response(id, 200, result, NULL)) {
        protocol_instance()->sendMcpMessage((uint8_t*)payload);
    }
    LOGD("json: %s\r\n", payload);
    os_free(payload);
}

static void mcp_server_reply_error(int id, char* message) 
{
    char* payload = psram_malloc(1024 * sizeof(char));
    os_snprintf(payload, 1024 * sizeof(char), "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"message\":\"%s\"}}", id, message);
    if (!mcp_server_reply_manager_response(id, 500, NULL, message)) {
        protocol_instance()->sendMcpMessage((uint8_t*)payload);
    }
    LOGD("json: %s\r\n", payload);
    os_free(payload);
}

static int mcp_server_add_tool(property_list_t* list)
{
    int ret = 0;
    property_list_t* node = list;
    mcp_server_t* server = mcp_server_instance();

    do{
        if(node == NULL)
        {
            LOGE("malloc failed\r\n");
            ret = -1;
            break;
        }

        dl_list_add_tail(&server->tools_head, &node->m_list);
    }while(0);

    return ret;
}

static void mcp_tool_call(int id, char* name, char* arg_str)
{
    mcp_cf_t info = {0};
    info.id = id;
    info.func_name = name;
    info.arg_json = arg_str;
    
    if (kNoErr != rtos_push_to_queue(&mcp_queue, &info, BEKEN_NO_WAIT)) 
    {
        LOGE("send msg fail \n");
        return;
    }
}

static void mcp_server_get_tool_list(int id, char* cursor)
{
    mcp_server_t* server = mcp_server_instance();
    property_list_t *tmp, *n;
    property_list_t *start_cursor = NULL;
    struct dl_list* start_head;
    int buf_len = sizeof(char)*1024*10;
    char* node_str = NULL;
    char* next_cursor = NULL;
    char* json_str = (char*)psram_malloc(buf_len);
    char* err_str = (char*)psram_malloc(1024*sizeof(char));
    char* json_ptr = json_str;

    sprintf(json_str, "{\"tools\":[");
    json_ptr += strlen(json_str);
    LOGD("id: %d, cursor: %s\r\n", id, cursor);

    do{
        if(cursor  == NULL || !strcmp(cursor, "\0"))
        {
            LOGI("return mcp tool first node\r\n");
            start_cursor = dl_list_first(&(server->tools_head), property_list_t, m_list);
            start_head = &server->tools_head;
        }
        else
        {
            dl_list_for_each_safe(tmp, n, &(server->tools_head), property_list_t, m_list)
            {
                LOGD("cursor: %s, name: %s\r\n", cursor, tmp->name);
                if(!strcmp(tmp->name, cursor))
                {
                    start_cursor = tmp;
                    LOGI("found cursor: %s\r\n", cursor);
                    start_head = start_cursor->m_list.prev;
                    break;
                }
            }
            if(start_cursor == NULL)
            {
                LOGE("no cursor in the list\r\n");
                sprintf(err_str, "tools/list: Failed to add tool %s because of can not find tool\n", cursor);
                server->reply_error(id, err_str);
                break;
            }
        }


        dl_list_for_each_safe(tmp, n, start_head, property_list_t, m_list)
        {
            if(tmp != NULL)
            {
                LOGD("name: %s, cursor: %s\r\n", tmp->name, start_cursor->name);
                cJSON* json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "name", tmp->name);
                cJSON_AddStringToObject(json, "description", tmp->description);

                cJSON* input_schema = cJSON_CreateObject();
                cJSON_AddStringToObject(input_schema, "type", "object");

                node_str = mcp_property_list_to_json(tmp);
                cJSON* properties = cJSON_Parse(node_str);
                cJSON_AddItemToObject(input_schema, "properties", properties);

                cJSON_AddItemToObject(json, "inputSchema", input_schema);
                char* tool_str = cJSON_PrintUnformatted(json);
                cJSON_Delete(json);
                
                if(node_str != NULL)
                {
                    if(strlen(json_str) + strlen(node_str) + 30 >= 8000)
                    {
                        LOGE("len out of 8000\r\n");
                        next_cursor = (char*)psram_malloc(256*sizeof(char));
                        sprintf(next_cursor, tmp->name);
                        cJSON_free(tool_str);
                        cJSON_free(node_str);
                        break;
                    }
                    else
                    {
                        LOGD("tool_str: %s\r\n", tool_str);
                        if(*(json_ptr -1) == '}')
                        {
                            sprintf(json_ptr, ",%s", tool_str);
                        }
                        else
                        {
                            sprintf(json_ptr, "%s", tool_str);
                        }
                        json_ptr = json_str + strlen(json_str);
                        cJSON_free(tool_str);
                        cJSON_free(node_str);
                    }
                }
            }
        }
        
        if(*(json_ptr - 1) == ',')
        {
            *(json_ptr - 1) = '\0';
        }
        
        if(next_cursor != NULL)
        {
            sprintf(json_ptr, "],\"nextCursor\":\"%s\"}", next_cursor);
            os_free(next_cursor);
        }
        else
        {
            sprintf(json_ptr, "]}");
        }
        server->reply_tool_list(id, json_str);
    }while(0);

    os_free(err_str);
    os_free(json_str);
    return;
}

static void mcp_server_prase_capability(cJSON* capability)
{
    bool got_url = false;

    do{
        cJSON* vision = cJSON_GetObjectItem(capability, "vision");
        if(vision == NULL)
        {
            LOGE("find vision err\r\n");
            break;
        }
    
        cJSON* url = cJSON_GetObjectItem(vision, "url");
        if(url == NULL || !cJSON_IsString(url))
        {
            LOGE("find url err\r\n");
            break;
        }
        else
        {
            os_snprintf(cap_url, sizeof(cap_url), "%s", url->valuestring);
            if (!mcp_server_vision_url_valid(cap_url)) {
                LOGW("reject invalid vision url:%s\r\n", cap_url);
                mcp_server_set_vision_fallback();
                LOGI("use vision fallback url:%s\r\n", cap_url);
            } else {
                LOGI("find url:%s\r\n", cap_url);
            }
            got_url = true;
        }

        cJSON* token = cJSON_GetObjectItem(vision, "token");
        if(token == NULL || !cJSON_IsString(token))
        {
            LOGE("find token err\r\n");
            break;
        }
        else
        {
            os_snprintf(cap_token, sizeof(cap_token), "%s", token->valuestring);
            LOGI("find token len:%d\r\n", os_strlen(cap_token));
        }
    }while(0);

    if (!got_url) {
        mcp_server_set_vision_fallback();
        LOGI("use vision fallback url:%s\r\n", cap_url);
    }
}

static void mcp_server_parse_message(cJSON* json)
{
    mcp_server_t* server = mcp_server_instance();
    cJSON *version = NULL;
    cJSON *method = NULL;
    cJSON *params = NULL;
    cJSON *id = NULL;

    if(json == NULL)
    {
        LOGE("mcp server parese msg err\r\n");
        return;
    }
    
    // Check JSONRPC version
    version = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
    if (version == NULL || !cJSON_IsString(version) || os_strcmp(version->valuestring, "2.0") != 0) 
    {
        LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        goto out;
    }

    // Check method
    method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (method == NULL || !cJSON_IsString(method)) 
    {
        LOGE(TAG, "Missing method");
        goto out;
    }

    const char *method_str = method->valuestring;
    if (os_strncmp(method_str, "notifications", 12) == 0) 
    {
        goto out;
    }

    // Check params
    params = cJSON_GetObjectItemCaseSensitive(json, "params");
    if (params != NULL && !cJSON_IsObject(params)) 
    {
        LOGE(TAG, "Invalid params for method: %s", method_str);
        goto out;
    }

    id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if (id == NULL || !cJSON_IsNumber(id)) 
    {
        LOGE(TAG, "Invalid id for method: %s", method_str);
        goto out;
    }

    int id_int = id->valueint;
    if (os_strcmp(method_str, "initialize") == 0) 
    {
        if (cJSON_IsObject(params)) {
            cJSON *capabilities = cJSON_GetObjectItemCaseSensitive(params, "capabilities");
            if (cJSON_IsObject(capabilities)) 
            {
                mcp_server_prase_capability(capabilities); // Implement this function in C
                //cJSON_Delete(capabilities);
            }
        }
        char message[256];
        os_snprintf(message, 256, 
                    "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
                    USER_AGENT_NAME, USER_AGENT_VER);
        server->reply_result(id_int, message);
    } 
    else if (os_strcmp(method_str, "tools/list") == 0) 
    {
        char *cursor_str = NULL;
        if (params != NULL) {
            cJSON *cursor = cJSON_GetObjectItemCaseSensitive(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = cursor->valuestring;
                //cJSON_Delete(cursor);
            }
        }
        LOGD("tools/list: cursor_str: %s\r\n", cursor_str);
        server->get_tool_list(id_int, cursor_str); // Implement this function in C
#if CONFIG_PROTOCOL_USE_MQTT
        extern void _protocol_send_client_hello(void);
        _protocol_send_client_hello();
#endif
    } 
    else if (os_strcmp(method_str, "tools/call") == 0) 
    {
        if (!cJSON_IsObject(params)) 
        {
            LOGE(TAG, "tools/call: Missing params");
            server->reply_error(id_int, "Missing params");
            goto out;
        }
        cJSON *tool_name = cJSON_GetObjectItemCaseSensitive(params, "name");
        if (!cJSON_IsString(tool_name)) 
        {
            LOGE(TAG, "tools/call: Missing name");
            server->reply_error(id_int, "Missing name");
            goto out;
        }
        cJSON *tool_arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments");
        if (tool_arguments != NULL && !cJSON_IsObject(tool_arguments)) 
        {
            LOGE(TAG, "tools/call: Invalid arguments");
            server->reply_error(id_int, "Invalid arguments");
            goto out;
        }
        char *name_str = psram_malloc(256);
        os_memset(name_str, 0, 256);
        char *arguments_str = cJSON_PrintUnformatted(tool_arguments);
        sprintf(name_str, "%s", tool_name->valuestring);
        mcp_tool_call(id_int, name_str, arguments_str);
        //cJSON_free(arguments_str);
        //DoToolCall(id_int, tool_name->valuestring, tool_arguments); // Implement this function in C
    } 
    else
    {
        LOGE(TAG, "Method not implemented: %s", method_str);
        char error_message[256];
        os_snprintf(error_message, 256, "Method not implemented: %s", method_str);
        server->reply_error(id_int, error_message);
    }

out:
#if 0
//delete json at web recv thred
    if(version != NULL){
        cJSON_Delete(version);
    }
    if(params != NULL){
        cJSON_Delete(params);
    }
    if(method != NULL){
        cJSON_Delete(method);
    }
    if(id != NULL){
        cJSON_Delete(id);
    }
#endif
    return;
}

static char* mcp_server_get_url(void)
{
    return cap_url;
}

static char* mcp_server_get_token(void)
{
    return cap_token;
}

static int mcp_server_init(void)
{
    int ret = 0;

    mcp_server_t *server = mcp_server_instance();
    ret = rtos_init_queue(&mcp_queue, "mcp_arguments",  sizeof(mcp_cf_t), 10);
    if(BK_OK != ret)
    {
        LOGE("[%s][%d] init queue fail\r\n", __FUNCTION__, __LINE__);
    }

    dl_list_init(&server->tools_head);

    return ret;
}

static int mcp_server_start(void)
{
    int ret = 0;

    do{
        ret = rtos_create_thread(&_call_func_thr, 4, "mcp_call_thr", mcp_server_call_function_main, 5*1024, NULL);
        if(ret != 0)
        {
            LOGE("create call func err\r\n");
            break;
        }

        
#if 1
        extern iot_devices_t* iot_devices_instance(void);
        iot_devices_instance()->super.init();
#endif
    }while(0);

    mcp_server_instance()->mcp_is_start = true;
    return ret;
}

static int mcp_server_deinit(void)
{
    int ret = 0;
    
    return ret;
}

static mcp_server_t g_mcp_server =
{
    .mcp_is_start = false,

    .super.init = mcp_server_init,
    .super.start = mcp_server_start,

    .recv_msg_cb = mcp_server_parse_message,
    .add_tool = mcp_server_add_tool,
    .get_tool_list = mcp_server_get_tool_list,
    .reply_error = mcp_server_reply_error,
    .reply_result = mcp_server_reply_result,
    .reply_tool_list = mcp_server_reply_tool_list,
    .set_manager_request_id = mcp_server_set_manager_request_id,
    .get_explain_url = mcp_server_get_url,
    .get_explain_token = mcp_server_get_token,
};

mcp_server_t *mcp_server_instance(void)
{
    return &g_mcp_server;
}

