#include "iot_devices.h"
#include "mcp_property.h"
#include "mcp_server.h"

#if (CONFIG_AUDIO_PLAYER)
bk_err_t app_audio_player_stop(void);
#endif

#define TAG "iot_player"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static void audio_player_reply(mcp_cf_t *msg, const char *text, bool is_error)
{
    cJSON *root = NULL;
    cJSON *content = NULL;
    cJSON *item = NULL;
    char *json_string = NULL;

    root = cJSON_CreateObject();
    content = cJSON_CreateArray();
    item = cJSON_CreateObject();
    if (root == NULL || content == NULL || item == NULL) {
        LOGE("create audio player reply failed\r\n");
        goto out;
    }

    cJSON_AddItemToObject(root, "content", content);
    cJSON_AddItemToArray(content, item);
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text ? text : "");
    cJSON_AddBoolToObject(root, "isError", is_error);

    json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        LOGE("print audio player reply failed\r\n");
        goto out;
    }

    if (is_error) {
        mcp_server_instance()->reply_error(msg->id, (char *)(text ? text : "audio player tool error"));
    } else {
        mcp_server_instance()->reply_result(msg->id, json_string);
    }

out:
    if (json_string != NULL) {
        cJSON_free(json_string);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
}

static void audio_player_stop_func_cb(mcp_cf_t *msg)
{
#if (CONFIG_AUDIO_PLAYER)
    bk_err_t ret = BK_OK;

    LOGI("audio player stop json: %s\r\n", msg->arg_json ? msg->arg_json : "");
    ret = app_audio_player_stop();
    if (ret == BK_OK) {
        audio_player_reply(msg, "true", false);
    } else {
        audio_player_reply(msg, "audio player stop failed", true);
    }
#else
    LOGI("audio player stop ignored, CONFIG_AUDIO_PLAYER disabled json: %s\r\n",
         msg->arg_json ? msg->arg_json : "");
    audio_player_reply(msg, "audio player is disabled", true);
#endif
}

int iot_audio_player_tool_init(void)
{
    property_list_t *stop_tool = NULL;
    mcp_server_t *server = mcp_server_instance();

    stop_tool = mcp_property_list_init();
    if (stop_tool == NULL) {
        return BK_FAIL;
    }

    stop_tool->name = "self.audio_player.stop";
    stop_tool->description = "Stop the current remote audio player playback.";
    stop_tool->func_cb = audio_player_stop_func_cb;
    server->add_tool(stop_tool);

    return BK_OK;
}
