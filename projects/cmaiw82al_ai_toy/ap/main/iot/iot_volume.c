#include "iot_devices.h"
#include "dialog_module.h"
#include "mcp_property.h"
#include "mcp_server.h"
#include "env_module.h"
#include "cJSON.h"

#define TAG "iot"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define VOL_ERR_RANGE       -2
#define VOL_SET_ERR         -1

static void iot_set_volume_func_cb(mcp_cf_t* msg)
{
    LOGI("json: %s\r\n", msg->arg_json);
    int val = 0;
    char* json_string;
    
    cJSON* arguments = cJSON_Parse(msg->arg_json);
    if (arguments == NULL) {
        LOGE("Error parsing JSON\n");
        return;
    }

    cJSON* volume = cJSON_GetObjectItem(arguments, "volume");
    if (volume == NULL) {
        LOGI("Error getting volume value\n");
        //cJSON_Delete(arguments);
        return;
    }

    val = volume->valueint;
    LOGI("Volume value: %d\n", val);

    
    cJSON *root = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();

    dialog_module_instance()->speaker_set_volume(val);

    cJSON_AddItemToArray(content, item);
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", "true");
    cJSON_AddItemToObject(root, "content", content);
    cJSON_AddBoolToObject(root, "isError", 0); // 0 表示 false

    json_string = cJSON_PrintUnformatted(root);
    
    mcp_server_instance()->reply_result(msg->id, json_string);
    cJSON_free(json_string);
    cJSON_Delete(root);
    cJSON_Delete(arguments);
    
    return;
}

static void iot_get_volume_func_cb(mcp_cf_t* msg)
{
    property_t node = {0};
    char* node_str = NULL;
    char* json_string;
    int volume = 0;

    node.name = "volume";
    node.type = PROPERTY_TYPE_INT;
    node.has_default_val = true;
    node.has_max_val = true;
    node.has_min_val = true;
    node.max_val = 100;
    node.min_val = 5;
    env_module_instance()->getSpkInfo(&volume);
    node.value.int_value = volume;
    LOGI("Volume value: %d\r\n", volume);
    node_str = mcp_property_to_json(&node);
    
    cJSON *root = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();

    cJSON_AddItemToArray(content, item);
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", node_str);

    cJSON_AddItemToObject(root, "content", content);
    cJSON_AddBoolToObject(root, "isError", 0); // 0 表示 false
    json_string = cJSON_PrintUnformatted(root);
    
    mcp_server_instance()->reply_result(msg->id, json_string);
    cJSON_free(json_string);
    cJSON_free(node_str);
    cJSON_Delete(root);
}

int iot_volume_tool_init(void)
{
    property_list_t* get_tool = NULL;
    property_list_t* set_tool = NULL;
    property_t node = {0};
    mcp_server_t* server = mcp_server_instance();

    node.name = "volume";
    node.type = PROPERTY_TYPE_INT;
    node.has_default_val = true;
    node.has_max_val = true;
    node.has_min_val = true;
    node.max_val = 100;
    node.min_val = 5;
    node.value.int_value = 45;

    get_tool = mcp_property_list_init();
    get_tool->name = "self.audio_speaker.get_volume";
    #if 1
    get_tool->description = "Provides the real-time information of the audio speaker\n"\
                           "Use this tool for:\n"\
                           "1.Answering questions about current condition"\
                           "(e.g. what is the current volume of the audio speaker?)\n"\
                           "2. As the first step to control the aud speaker";
    #else
    get_tool->description = "test";
    #endif
    get_tool->func_cb = iot_get_volume_func_cb;
    mcp_property_list_add_node(get_tool, &node);
    server->add_tool(get_tool);

    set_tool = mcp_property_list_init();
    set_tool->name = "self.audio_speaker.set_volume";
    #if 1
    set_tool->description = "Set the volume of the audio speaker."\
                           "If the current volume is unknown,"\
                           "you must call self.audio_speaker.get_volume"\
                           "and then call this tool";
    #else
    set_tool->description = "test";
    #endif
    set_tool->func_cb = iot_set_volume_func_cb;
    mcp_property_list_add_node(set_tool, &node);
    server->add_tool(set_tool);

    return 0;
}

