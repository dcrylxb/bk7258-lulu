#ifndef __OTA_MODULE_H__
#define __OTA_MODULE_H__

#include "common.h"

#define CONFIG_OTA_VERSION_URL XIAOZHI_OTA_VERSION_URL

typedef struct{
    super_module_t super;
    bool (*isActive)(void);

    char* (*getUrl)(void);

#if CONFIG_PROTOCOL_USE_WSS
    char* (*getToken)(void);
#endif
#if CONFIG_PROTOCOL_USE_MQTT
    char* (*getClientId)(void);
    char* (*getUsername)(void);
    char* (*getPassword)(void);
    char* (*getPubTopic)(void);
#endif
}ota_module_t;

ota_module_t *ota_instance(void);

#endif
