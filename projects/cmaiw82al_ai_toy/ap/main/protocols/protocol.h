#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>

#include <components/shell_task.h>
#include "FreeRTOS.h"
#include "task.h"

#include "event_groups.h"

#include "cli.h"
#include "common.h"

typedef struct{
    bool m_is_init;
    bool m_is_start;

    super_module_t super;
    void (*sendAudio)(uint8_t*, int);
    void (*sendWakeJson)(uint8_t*);
    void (*sendStartListen)(uint8_t*);
    void (*sendStopListen)(void);
    void (*sendAbortListen)(uint8_t*);
#if CONFIG_PROTOCOL_IOT_MCP
    void (*sendMcpMessage)(uint8_t*);
#endif
}pro_module_t;

static void _protocol_send_audio(uint8_t* opus, int len);
static void _protocol_send_wake_detected(uint8_t* wake_word);
static void _protocol_send_start_listening(uint8_t* mode);
static void _protocol_send_stop_listening(void);
static void _protocol_send_abort_listening(uint8_t* desc);
static int _protocol_client_start(void);
static int _protocol_cient_init(void);
static int _protool_client_stop(void);
static int _protocol_client_deinit(void);

pro_module_t *protocol_instance(void);

#endif
