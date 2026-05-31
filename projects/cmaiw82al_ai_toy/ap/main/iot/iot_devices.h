#ifndef __IOT_DEVICES_H__
#define __IOT_DEVICES_H__

#include <os/os.h>
#include "common.h"

typedef struct{
    super_module_t super;
}iot_devices_t;

#if (CONFIG_IOT_DEV_CAMERA)
#include "iot_camera.h"
#endif

int iot_volume_tool_init(void);
int iot_audio_player_tool_init(void);
int iot_pet_tool_init(void);
iot_devices_t* iot_devices_instance(void);

#endif
