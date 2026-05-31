#ifndef __NET_CONFIG_H__
#define __NET_CONFIG_H__

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

#include "apconfig_example.h"

#include <modules/wifi.h>
#include <components/event.h>
#include <components/netif.h>
#include <string.h>
#include <components/log.h>

#include "webnet.h"
#include "wn_module.h"
#include "apconfig_example.h"

#include "components/log.h"

#define NET_TAG "net_config"
#define NET_LOGI(...) BK_LOGW(NET_TAG, ##__VA_ARGS__)
#define NET_LOGW(...) BK_LOGW(NET_TAG, ##__VA_ARGS__)
#define NET_LOGE(...) BK_LOGE(NET_TAG, ##__VA_ARGS__)
#define NET_LOGD(...) BK_LOGD(NET_TAG, ##__VA_ARGS__)

typedef struct{
    bool                       m_is_config;                //是否是配网后启动的sta
    super_module_t             super;
    net_info_t                 m_net_info;
    bool m_ble_provisioning_ready;
    bool m_ble_provisioning_active;
    int (*setNetInfo)(net_info_t*);

    s32 (*connect_ap)(char *ssid, char *password, bool is_config);
    int (*start_ble_provisioning)(void);
}net_config_t;

net_config_t* net_config_instance(void);

#endif
