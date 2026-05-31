#include "iot_devices.h"
#include "mcp_property.h"
#include "mcp_server.h"
#include "pet_brain.h"
#include "pet_action_router.h"

#define TAG "iot_pet"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static void pet_reply_text(int id, const char *text, bool is_error)
{
    cJSON *root = NULL;
    cJSON *content = NULL;
    cJSON *item = NULL;
    char *json_string = NULL;

    root = cJSON_CreateObject();
    content = cJSON_CreateArray();
    item = cJSON_CreateObject();
    if (root == NULL || content == NULL || item == NULL) {
        LOGE("create reply json failed\r\n");
        goto out;
    }

    cJSON_AddItemToObject(root, "content", content);
    cJSON_AddItemToArray(content, item);
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text ? text : "");
    cJSON_AddBoolToObject(root, "isError", is_error);

    json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        LOGE("print reply json failed\r\n");
        goto out;
    }

    if (is_error) {
        mcp_server_instance()->reply_error(id, (char *)(text ? text : "pet tool error"));
    } else {
        mcp_server_instance()->reply_result(id, json_string);
    }

out:
    if (json_string != NULL) {
        cJSON_free(json_string);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
}

static void pet_get_state_func_cb(mcp_cf_t *msg)
{
    pet_brain_snapshot_t snapshot = {0};
    cJSON *root = NULL;
    char *state_json = NULL;

    pet_brain_get_snapshot(&snapshot);

    root = cJSON_CreateObject();
    if (root == NULL) {
        pet_reply_text(msg->id, "pet state unavailable", true);
        return;
    }

    cJSON_AddStringToObject(root, "state", pet_brain_state_name(snapshot.state));
    cJSON_AddBoolToObject(root, "privacy", snapshot.privacy);
    cJSON_AddNumberToObject(root, "mood", snapshot.mood);
    cJSON_AddNumberToObject(root, "energy", snapshot.energy);
    cJSON_AddNumberToObject(root, "affection", snapshot.affection);
    cJSON_AddNumberToObject(root, "security", snapshot.security);
    cJSON_AddNumberToObject(root, "novelty", snapshot.novelty);
    cJSON_AddNumberToObject(root, "stress", snapshot.stress);
    cJSON_AddStringToObject(root, "last_event", pet_brain_event_name(snapshot.last_event));
    cJSON_AddStringToObject(root, "last_action", pet_action_router_action_name(snapshot.last_action));
    cJSON_AddStringToObject(root, "last_emotion", snapshot.last_emotion);

    state_json = cJSON_PrintUnformatted(root);
    if (state_json == NULL) {
        pet_reply_text(msg->id, "pet state unavailable", true);
    } else {
        pet_reply_text(msg->id, state_json, false);
        cJSON_free(state_json);
    }

    cJSON_Delete(root);
}

static void pet_emit_action_func_cb(mcp_cf_t *msg)
{
    cJSON *root = NULL;
    pet_action_request_t request = {0};
    bk_err_t ret = BK_OK;

    LOGI("pet_action json: %s\r\n", msg->arg_json ? msg->arg_json : "");

    root = cJSON_Parse(msg->arg_json);
    if (root == NULL) {
        pet_reply_text(msg->id, "invalid pet_action json", true);
        return;
    }

    ret = pet_action_router_request_from_json(root, &request);
    if (ret == BK_OK) {
        ret = pet_brain_apply_action_request(&request);
    }
    cJSON_Delete(root);

    if (ret == BK_OK) {
        pet_reply_text(msg->id, "true", false);
    } else {
        pet_reply_text(msg->id, "pet_action rejected", true);
    }
}

int iot_pet_tool_init(void)
{
    property_list_t *get_tool = NULL;
    property_list_t *emit_tool = NULL;
    property_t node = {0};
    mcp_server_t *server = mcp_server_instance();

    get_tool = mcp_property_list_init();
    if (get_tool == NULL) {
        return BK_FAIL;
    }
    get_tool->name = "self.pet.get_state";
    get_tool->description = "Get the local AI pet state. Use this before choosing a pet action.";
    get_tool->func_cb = pet_get_state_func_cb;
    server->add_tool(get_tool);

    emit_tool = mcp_property_list_init();
    if (emit_tool == NULL) {
        return BK_FAIL;
    }
    emit_tool->name = "self.pet.emit_action";
    emit_tool->description = "Emit a bounded semantic pet action. Allowed actions are reply, emote, soothe, sleep, privacy_on, privacy_off, and abort. Never use this for raw GPIO, PWM, flash, or display control.";
    emit_tool->func_cb = pet_emit_action_func_cb;

    node.name = "action";
    node.type = PROPERTY_TYPE_STR;
    node.has_default_val = true;
    node.value.string_value = "emote";
    mcp_property_list_add_node(emit_tool, &node);

    node.name = "emotion";
    node.type = PROPERTY_TYPE_STR;
    node.has_default_val = true;
    node.value.string_value = "smiling";
    mcp_property_list_add_node(emit_tool, &node);

    node.name = "duration_ms";
    node.type = PROPERTY_TYPE_INT;
    node.has_default_val = true;
    node.has_max_val = true;
    node.has_min_val = true;
    node.value.int_value = 2000;
    node.max_val = PET_ACTION_MAX_DURATION_MS;
    node.min_val = 0;
    mcp_property_list_add_node(emit_tool, &node);

    server->add_tool(emit_tool);

    return BK_OK;
}
