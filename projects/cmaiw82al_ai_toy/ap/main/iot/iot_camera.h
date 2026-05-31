#ifndef __IOT_CAMERA_H__
#define __IOT_CAMERA_H__

#include <stdint.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_IOT_DEV_CAMERA
bk_err_t iot_camera_capture_jpeg(uint8_t **jpg_data, uint32_t *jpg_len);
bk_err_t iot_camera_explain_jpeg(uint8_t *jpg_data, uint32_t jpg_len,
                                 const char *question, char **result);
bk_err_t iot_camera_take_photo_and_explain(const char *question, char **result);
bool iot_camera_vision_ready(void);
int iot_camera_tool_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
