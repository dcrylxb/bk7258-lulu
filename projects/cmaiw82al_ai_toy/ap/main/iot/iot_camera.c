#include <os/os.h>
#include <os/str.h>
#include <stdio.h>
#if CONFIG_IOT_DEV_CAMERA
#include <components/log.h>
#include <components/netif.h>
#include <bk_websocket_client.h>
#include <components/webclient.h>
#include "cJSON.h"

#include "common.h"
#include "iot_devices.h"
#include "iot_camera.h"
#include "mcp_property.h"
#include "mcp_server.h"
#include "boards_common.h"
#include "bk_app_camera.h"
#include "pet_scene.h"

#define TAG "iot"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define CAM_BOUNDARY        "----BEKEN_CAMERA_BOUNDARY"

#define HTTP_HDR_BUF_SIZE   (1024)
#define HTTP_RESP_BUF_SIZE  (4096)
#define MULTIPART_BUF_SIZE  (HTTP_HDR_BUF_SIZE - 256)

extern int webclient_handle_response(struct webclient_session *session);
extern int webclient_connect(struct webclient_session *session, const char *URI);
int webclient_send_header(struct webclient_session *session, int method);

bk_err_t iot_camera_capture_jpeg(uint8_t **jpg_data, uint32_t *jpg_len)
{
    bk_err_t ret = BK_FAIL;
    uint8_t *data = NULL;
    uint32_t len = 0;
    frame_buffer_t *frame = NULL;

    app_camera_config_t cam_cfg = DEFAULT_APP_CAMERA_CONFIG();
    cam_cfg.count = 1;
    cam_cfg.i2c_id = CAMERA_I2C_ID;
    cam_cfg.pwr_pin = CAMERA_PWR_GPIO;
    cam_cfg.rst_pin = CAMERA_RST_GPIO;
    
    ret = bk_app_camera_open(&cam_cfg);
    if (ret != BK_OK){
        LOGE("open dvp camera fail, ret: %d\r\n", ret);
        return ret;
    }

    frame = bk_app_camera_get_frame(2000);
    if (frame == NULL) {
        LOGE("capture image fail\r\n");
        goto out;
    }
    LOGD("frame info, width: %d, height: %d, size: %d, len: %d\r\n", 
        frame->width, frame->height, frame->size, frame->length);

    len = frame->length;
    data = psram_malloc(len);
    if (data == NULL) {
        LOGE("malloc fail, size: %d\r\n", len);
        goto out;
    }
    
    os_memcpy(data, frame->frame, len);

    *jpg_data = data;
    *jpg_len = len;

    ret = BK_OK;

out:
    if (frame) {
        bk_app_camera_free_frame(frame);
    }

    bk_app_camera_close();

    return ret;
}

static void _question_fileder_string_init(char* question_fileder, const char* question)
{
    os_snprintf(question_fileder, MULTIPART_BUF_SIZE,
                               "--%s\r\n"\
                               "Content-Disposition: form-data; name=\"question\"\r\n"\
                               "\r\n"\
                               "%s\r\n",
                               CAM_BOUNDARY,
                               question
    );
    LOGI("%s\r\n", question_fileder);
}

static void _file_header_string_init(char* file_header)
{
    os_snprintf(file_header, MULTIPART_BUF_SIZE,
                          "--%s\r\n"\
                          "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n"\
                          "Content-Type: image/jpeg\r\n"\
                          "\r\n",
                          CAM_BOUNDARY
    );
    LOGI("%s\r\n", file_header);
}

static void _multipart_footer_string_init(char* multipart_footer)
{
    os_snprintf(multipart_footer, MULTIPART_BUF_SIZE,
                          "\r\n--%s--\r\n",
                          CAM_BOUNDARY
    );
    LOGI("%s\r\n", multipart_footer);
}

static void _rpc_reply(int id, char *data)
{
    cJSON *root = NULL;
    cJSON *content = NULL;
    cJSON *item = NULL;
    char *reply_str = NULL;

    root = cJSON_CreateObject();
    if (root == NULL) {
        LOGE("create cJSON object fail\r\n");
        return;
    }

    content = cJSON_CreateArray();
    if (content == NULL) {
        LOGE("create cJSON array fail\r\n");
        goto out;
    }
    cJSON_AddItemToObject(root, "content", content);

    item = cJSON_CreateObject();
    if (item == NULL) {
        LOGE("create cJSON object fail\r\n");
        goto out;
    }
    cJSON_AddItemToArray(content, item);

    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", data);
    cJSON_AddBoolToObject(root, "isError", 0); // 表示成功

    reply_str = cJSON_PrintUnformatted(root);
    if (reply_str == NULL) {
        LOGE("cJSON print fail\r\n");
        goto out;
    }

    mcp_server_instance()->reply_result(id, reply_str);

out:
    if (root) {
        cJSON_Delete(root);
    }

    if (reply_str) {
        cJSON_free(reply_str);
    }

    return;
}

static int _write_chunk_data(struct webclient_session *session, uint8_t *data, uint32_t len)
{
    int ret = 0;
    char chunk_size[16] = {0};

    os_snprintf(chunk_size, sizeof(chunk_size), "%X\r\n", len);
    
    ret = webclient_write(session, chunk_size, strlen(chunk_size));
    if (ret <= 0) {
        return BK_FAIL;
    }

    ret = webclient_write(session, data, len);
    if (ret <= 0) {
        return BK_FAIL;
    }

    ret = webclient_write(session, (void*)"\r\n", 2);
    if (ret <= 0) {
        return BK_FAIL;
    }

    return BK_OK;
}

bk_err_t iot_camera_explain_jpeg(uint8_t *jpg_data, uint32_t jpg_len,
                                 const char *question, char **result)
{
    int ret = BK_FAIL;
    int bytes_read = 0;
    int resp_status = 0;
    char *buffer = NULL;
    char chunk_buf[128] = {0};
    char *multipart_data = NULL;
    struct webclient_session *session = NULL;

    if (jpg_data == NULL || jpg_len == 0 || question == NULL || result == NULL) {
        LOGE("invalid explain params\r\n");
        return BK_ERR_PARAM;
    }

    if (!iot_camera_vision_ready()) {
        LOGE("vision url/token not ready\r\n");
        return BK_FAIL;
    }

    session = webclient_session_create(HTTP_HDR_BUF_SIZE);
    if (session == NULL) {
        LOGE("create session failed.\n");
        return BK_FAIL;
    }

    multipart_data = psram_malloc(MULTIPART_BUF_SIZE);
    if (multipart_data == NULL) {
        LOGE("malloc fail, size: %d\r\n", MULTIPART_BUF_SIZE);
        goto out;
    }
    os_memset(multipart_data, 0, MULTIPART_BUF_SIZE);

    webclient_header_fields_add(session, "Transfer-Encoding: chunked\r\n");
    webclient_header_fields_add(session, "User-Agent: %s/%s\r\n", USER_AGENT_NAME, USER_AGENT_VER);
    webclient_header_fields_add(session, "Device-Id: %s\r\n", board_instance()->getMac());
    webclient_header_fields_add(session, "Client-Id: %s\r\n", board_instance()->getUid());
    //webclient_header_fields_add(session, "Content-Type: application/json\r\n");
    webclient_header_fields_add(session, "Authorization: Bearer %s\r\n", mcp_server_instance()->get_explain_token());
    webclient_header_fields_add(session, "Content-Type: multipart/form-data; boundary=%s\r\n", CAM_BOUNDARY);

    ret = webclient_connect(session, mcp_server_instance()->get_explain_url());
    if (ret != 0) {
        LOGE("connect vision server failed\n");
        goto out;
    }

    ret = webclient_send_header(session, WEBCLIENT_POST);
    if (ret != 0) {
        LOGE("send http header failed\n");
        goto out;
    }

    // send question
    os_memset(multipart_data, 0, MULTIPART_BUF_SIZE);
    _question_fileder_string_init(multipart_data, question);
    ret = _write_chunk_data(session, (uint8_t *)multipart_data, strlen(multipart_data));
    if (ret != BK_OK) {
        LOGE("write question chunk data failed\n");
        goto out;
    }

    // send file
    os_memset(multipart_data, 0, MULTIPART_BUF_SIZE);
    _file_header_string_init(multipart_data);
    ret = _write_chunk_data(session, (uint8_t *)multipart_data, strlen(multipart_data));
    if (ret != BK_OK) {
        LOGE("write file header chunk data failed\n");
        goto out;
    }
    
    // send image
    LOGD("send jpg image, data_len: %d\r\n", jpg_len);
    ret = _write_chunk_data(session, jpg_data, jpg_len);
    if (ret != BK_OK) {
        LOGE("write image chunk data failed\n");
        goto out;
    }

    // send footer
    os_memset(multipart_data, 0, MULTIPART_BUF_SIZE);
    _multipart_footer_string_init(multipart_data);
    ret = _write_chunk_data(session, (uint8_t *)multipart_data, strlen(multipart_data));
    if (ret != BK_OK) {
        LOGE("write footer chunk data failed\n");
        goto out;
    }

    webclient_write(session, (void*)"0\r\n\r\n", 5);

    resp_status = webclient_handle_response(session);
    LOGI("http response, status code: %d\r\n", resp_status);
    if (resp_status != 200) {
        LOGE("upload photo failed\r\n");
        goto out;
    }

    buffer = psram_malloc(HTTP_RESP_BUF_SIZE);
    if (buffer == NULL) {
        LOGE("malloc fail, size: %d\r\n", HTTP_RESP_BUF_SIZE);
        goto out;
    }
    os_memset(buffer, 0, HTTP_RESP_BUF_SIZE);

    do {
        os_memset(chunk_buf, 0, sizeof(chunk_buf));
        bytes_read = webclient_read(session, chunk_buf, sizeof(chunk_buf));
        if (bytes_read <= 0) {
            break;
        }

        if (strlen(buffer) + bytes_read >= HTTP_RESP_BUF_SIZE) {
            LOGE("response buffer overflow\r\n");
            break;
        }

        strncat(buffer, chunk_buf, bytes_read);
    } while (1);

    *result = buffer;

    ret = BK_OK;

out:
    if (multipart_data) {
        psram_free(multipart_data);
        multipart_data = NULL;
    }

    webclient_close(session);

    return ret;
}

bk_err_t iot_camera_take_photo_and_explain(const char *question, char **result)
{
    bk_err_t ret = BK_FAIL;
    uint8_t *jpg_data = NULL;
    uint32_t jpg_len = 0;

    if (question == NULL || result == NULL) {
        return BK_ERR_PARAM;
    }

    *result = NULL;

    ret = pet_scene_handle_event(PET_EVENT_VISION_CAPTURE_START);
    if (ret != BK_OK) {
        LOGW("vision scene rejected ret=%d\r\n", ret);
        goto out_without_scene;
    }

    ret = iot_camera_capture_jpeg(&jpg_data, &jpg_len);
    if (ret != BK_OK) {
        LOGE("capture jpeg failed:%d\r\n", ret);
        goto out;
    }

    ret = iot_camera_explain_jpeg(jpg_data, jpg_len, question, result);
    if (ret != BK_OK) {
        LOGE("explain jpeg failed:%d\r\n", ret);
    }

out:
    pet_scene_handle_event((ret == BK_OK) ? PET_EVENT_VISION_CAPTURE_DONE : PET_EVENT_VISION_CAPTURE_ERROR);

    if (jpg_data) {
        psram_free(jpg_data);
    }

out_without_scene:
    return ret;
}

bool iot_camera_vision_ready(void)
{
    char *url = mcp_server_instance()->get_explain_url();
    char *token = mcp_server_instance()->get_explain_token();

    if (url == NULL || url[0] == '\0' ||
        token == NULL || token[0] == '\0') {
        return false;
    }

    if ((os_strncmp(url, "http://", 7) != 0 &&
         os_strncmp(url, "https://", 8) != 0) ||
        os_strstr(url, "dashscope.aliyuncs.com") != NULL ||
        os_strstr(url, "/compatible-mode/") != NULL ||
        os_strstr(url, "/xiaozhi/api/vision") == NULL) {
        LOGW("vision endpoint invalid:%s\r\n", url);
        return false;
    }

    return true;
}

static void iot_call_camera_func_cb(mcp_cf_t* msg)
{
    int ret = BK_OK;
    char *resp_text = NULL;
    char *result = NULL;
    cJSON *arg_json = NULL;
    cJSON *question_json = NULL;
    char *question = NULL;

    if (msg == NULL || msg->arg_json == NULL) {
        LOGE("msg is null\n");
        return;
    }

    arg_json = cJSON_Parse(msg->arg_json);
    if (arg_json == NULL) {
        LOGE("arg_json parse fail\n");
        resp_text = "{\"success\": false, \"message\": \"parse arg_json failed\"}";
        return;
    }

    question_json = cJSON_GetObjectItem(arg_json, "question");
    if (question_json == NULL || question_json->valuestring == NULL) {
        LOGE("question is missing, use default question\n");
        question = "请详细描述这张图片中的内容，包括主要物体、场景、颜色，如果有人物，请描述他们的动作和表情。";
    } else {
        question = question_json->valuestring;
    }

    ret = iot_camera_take_photo_and_explain(question, &result);
    if (ret != BK_OK) {
        LOGE("image explain failed\r\n");
        resp_text = "{\"success\": false, \"message\": \"image explain failed\"}";
        goto out;
    }

    resp_text = result;

out:
    if (arg_json) {
        cJSON_Delete(arg_json);
        arg_json = NULL;
    }

    _rpc_reply(msg->id, resp_text);

    if (result) {
        psram_free(result);
        result = NULL;
    }

    return;
}

int iot_camera_tool_init(void)
{
    property_list_t *tool = NULL;
    property_t node = {0};
    
    do {
        mcp_server_t* server = mcp_server_instance();

        node.name = "question";
        node.type = PROPERTY_TYPE_STR;
        tool = mcp_property_list_init();
        tool->name = "self.camera.take_photo";
        tool->description = "Take a photo and explain it. Use this tool after the user asks you to see something.\n"\
                               "Args:\n"\
                               "1.Answering questions about current condition"\
                               " `question`: The question that you want to ask about the photo.\n"\
                               "Return:\n";
                               " A JSON object that provides the photo information.";

        tool->func_cb = iot_call_camera_func_cb;
        mcp_property_list_add_node(tool, &node);

        server->add_tool(tool);
    } while (0);

    return 0;
}
#endif
