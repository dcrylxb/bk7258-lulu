#ifndef __ENV_MODULE_H__
#define __ENV_MODULE_H__

#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include "apconfig_example.h"
#include "common.h"

#define SYS_DATA_WIFI_INFO             "wifi_info"
#define SYS_DATA_SPK_INFO              "spk_info"

typedef struct{
    super_module_t super;
    int (*getNetInfo)(net_info_t*);
    int (*setNetInfo)(net_info_t*);
    void (*delNetInfo)(void);
    int (*getSpkInfo)(int*);
    int (*setSpkInfo)(int*);
    void (*delSpkInfo)(void);
}env_module_t;

env_module_t* env_module_instance(void);
#endif
