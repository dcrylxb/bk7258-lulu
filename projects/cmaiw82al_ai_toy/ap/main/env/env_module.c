#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include "easyflash.h"
#include "env_module.h"
#include "apconfig_example.h"
#if CONFIG_BK_NETWORK_PROVISIONING
#include "bk_network_provisioning.h"
#endif
#if (CONFIG_EASY_FLASH && CONFIG_EASY_FLASH_V4)
#include "bk_ef.h"
#endif

#define TAG "env"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static net_info_t g_net_info = {0};

static int _env_get_net_config(net_info_t* net_cfg)
{
    bk_err_t ret = BK_FAIL;

    do{
        if((!net_cfg)){
            LOGE("param is NULL\r\n");
        }

        if(0 == os_strlen(g_net_info.ssid)){
            if(bk_get_env_enhance(SYS_DATA_WIFI_INFO, &g_net_info, sizeof(net_info_t)) <= 0)
            {
                LOGE("get wifi_info fail\r\n");
                ret = BK_FAIL;
            }
            else{
                ret = BK_OK;
            }

        }
        else{
            ret = BK_OK;
        }

        if(BK_OK == ret){
            os_memcpy(net_cfg, &g_net_info, sizeof(net_info_t));
            LOGI("ssid: %s, password: %s\n", net_cfg->ssid, net_cfg->pwd);
        }
    }while(0);

    return ret;
}

static int _env_set_net_config(net_info_t *net_info)
{
    bk_err_t ret = BK_OK;

    if(!net_info)
    {
        LOGE("param is NULL\r\n");
        return BK_FAIL;
    }
    
    if(0 != os_memcmp(net_info, &g_net_info, sizeof(net_info)))
    {
        ret = bk_set_env_enhance(SYS_DATA_WIFI_INFO, net_info, sizeof(net_info_t));
        if(BK_OK == ret)
        {
            os_memcpy(&g_net_info, net_info, sizeof(net_info_t));
            LOGI("set wifi_info success\r\n");
        }
        else
        {
            LOGE("set wifi_info fail, ret: %d\r\n", ret);
        }
    }

    return ret;
}

static void _env_del_net_info(void)
{
    os_memset(&g_net_info, 0, sizeof(g_net_info));
    bk_set_env_enhance(SYS_DATA_WIFI_INFO, NULL, 0);
#if CONFIG_BK_NETWORK_PROVISIONING
    erase_network_auto_reconnect_info();
#endif
    LOGI("delete wifi_info success\r\n");
}

static int _env_get_spk_config(int* val)
{
    bk_err_t ret = BK_FAIL;

    do{
        if(bk_get_env_enhance(SYS_DATA_SPK_INFO, val, sizeof(int)) <= 0)
        {
            LOGE("get spk_info fail\r\n");
            ret = BK_FAIL;
        }
        else{
            LOGI("get spk_info success, val: %d\r\n", *val);
            ret = BK_OK;
        }
    }while(0);

    return ret;
}

static int _env_set_spk_config(int* val)
{
    bk_err_t ret = BK_OK;

    ret = bk_set_env_enhance(SYS_DATA_SPK_INFO, val, sizeof(int));
    if(BK_OK == ret)
    {
        LOGI("set spk_info success, val: %d\r\n", *val);
    }
    else
    {
        LOGE("set spk_info fail, ret: %d\r\n", ret);
    }

    return ret;
}

static void _env_del_spk_info(void)
{
    bk_set_env_enhance(SYS_DATA_SPK_INFO, NULL, 0);
}


static int _env_module_init(void)
{
    easyflash_init(); 
    return 0;
}


static int _env_module_deinit(void)
{
    return 0;
}

static env_module_t g_env_module = 
{
    .super.init = _env_module_init,
    .super.deinit = _env_module_deinit,
    .setNetInfo = _env_set_net_config,
    .getNetInfo = _env_get_net_config,
    .delNetInfo = _env_del_net_info,
    .setSpkInfo = _env_set_spk_config,
    .getSpkInfo = _env_get_spk_config,
    .delSpkInfo = _env_del_net_info,
};

env_module_t* env_module_instance(void)
{
    return &g_env_module;
}

