#ifndef __PROTOCOL_WEBSOCKET_H__
#define __PROTOCOL_WEBSOCKET_H__

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>

#include <components/shell_task.h>
#include "FreeRTOS.h"
#include "task.h"

#include "event_groups.h"

#include "cli.h"
#include "cJSON.h"
#include "common.h"
#include <bk_websocket_client.h>

typedef struct{
    transport wss_client;

    super_module_t super;
    void (*sendAudio)(uint8_t*, int);
    void (*sendText)(uint8_t*);
    char* (*getSessionId)(void);
}pws_module_t;

pws_module_t *protocol_websocket_instance(void);

#define WEB_HEAD_LEN            1024

#if !CONFIG_WSS_INFO_BY_USER
#define WEB_HEADER_TEXT \
    "Authorization: Bearer %s\r\n"\
    "Protocol-Version: 1\r\n"\
    "Device-Id: %s\r\n"\
    "Client-Id: %s\r\n"
#else
#define WEB_HEADER_TEXT \
    "Authorization: Bearer test-token\r\n"\
    "Protocol-Version: 1\r\n"\
    "Device-Id: %s\r\n"\
    "Client-Id: %s\r\n"

#endif

#define WEBRECV_TASK_PRIORITY        5

#endif
