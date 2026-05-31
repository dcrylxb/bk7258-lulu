#include "iot_devices.h"

static int _iot_devices_init(void)
{
    iot_volume_tool_init();
    iot_audio_player_tool_init();
    iot_pet_tool_init();
#if CONFIG_IOT_DEV_CAMERA
    iot_camera_tool_init();
#endif
    return 0;
}

static iot_devices_t g_iot_module = 
{
    .super.init = _iot_devices_init,
};

iot_devices_t* iot_devices_instance(void)
{
    return &g_iot_module;
}

