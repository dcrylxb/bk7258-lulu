#ifndef __PROTOCOL_MQTT_H__
#define __PROTOCOL_MQTT_H__

#include <components/system.h>
#include <os/os.h>

#include "common.h"
#include "paho_mqtt.h"
#include "paho_mqtt_udp.h"

typedef struct{
    MQTT_CLIENT_T mqtt_client;
    bool is_start;

    super_module_t super;
    void (*sendAudio)(uint8_t*, int);
    void (*sendText)(uint8_t*);
    char* (*getSessionId)(void);
}mqtt_module_t;

mqtt_module_t * mqtt_module_instance(void);


#endif
