#include <os/os.h>
#include <os/mem.h>
#include <components/bk_uid.h>
#include <stdio.h>

#include "boards_common.h"
#include "cJSON.h"
#include "common.h"

#include "modules/pm.h"
#include <driver/pwr_clk.h>

#include "bk_gpio.h"
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"
#include "bk_wifi.h"

#define TAG "boards"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static char* g_post_data = NULL;
static char g_dev_mac[24] = {0};
static char g_dev_uid[128] = {0};
static char g_dev_sub_mac[24] = {0};
extern volatile const uint8_t build_version[];

char* board_get_uuid(void)
{
    uint8_t base_mac[24] = {0};
    // bk_get_mac(base_mac, MAC_TYPE_BASE);
    get_net_info(WIFI_MAC_ITEM, base_mac, NULL, NULL);
    snprintf(g_dev_uid, 128, "e6a322d5-99af-4af1-b398-%02x%02x%02x%02x%02x%02x",
            base_mac[0]-1,base_mac[1],base_mac[2],base_mac[3],base_mac[4],base_mac[5]);
    LOGI("uid:%s\r\n", g_dev_uid);
    return g_dev_uid;
}

char* board_get_mac(void)
{
    uint8_t base_mac[6] = {0};
    get_net_info(WIFI_MAC_ITEM, base_mac, NULL, NULL);
    // bk_get_mac(base_mac, MAC_TYPE_BASE);
    snprintf(g_dev_mac, 24, "%02x:%02x:%02x:%02x:%02x:%02x",
            base_mac[0]-1,base_mac[1],base_mac[2],base_mac[3],base_mac[4],base_mac[5]);
    LOGI("mac:%s\r\n", g_dev_mac);
    return g_dev_mac;
}

char* board_get_sub_mac(void)
{
    uint8_t base_mac[24] = {0};
    get_net_info(WIFI_MAC_ITEM, base_mac, NULL, NULL);
    // bk_get_mac(base_mac, MAC_TYPE_BASE);
    snprintf(g_dev_sub_mac, 24, "%02x_%02x_%02x_%02x_%02x_%02x",
            base_mac[0]-1,base_mac[1],base_mac[2],base_mac[3],base_mac[4],base_mac[5]);
    LOGI("sub_mac:%s\r\n", g_dev_sub_mac);
    return g_dev_sub_mac;
}

char* board_get_json_str(void)
{
    /*return board version name... data*/
    cJSON *root = cJSON_CreateObject();

    // 添加flash_size
    //cJSON_AddNumberToObject(root, "flash_size", 16777216);

    // 添加minimum_free_heap_size
    //cJSON_AddNumberToObject(root, "minimum_free_heap_size", 8457848);

    // 添加mac_address
    cJSON_AddStringToObject(root, "mac_address", (const char*)board_get_mac());

    // 添加chip_model_name
    cJSON_AddStringToObject(root, "chip_model_name", "bk7258");

    // 添加application
    cJSON *application = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "application", application);

    // 添加application的子项
    cJSON_AddStringToObject(application, "name", "xiaozhi");
    cJSON_AddStringToObject(application, "version", USER_AGENT_VER);
    cJSON_AddStringToObject(application, "compile_time", (const char*)build_version);
    cJSON_AddStringToObject(application, "bk_avdk_smp", "v3.1.1.5");

    g_post_data = cJSON_PrintUnformatted(root);
    LOGI("post_data:\r\n%s\r\n", g_post_data);

    cJSON_Delete(root);

    return g_post_data;
}

static void _boards_enter_deep_sleep(void)
{
    return;
}

static int _boards_init(void)
{
    gpio_dev_unmap(HW_LDO_GPIO);
    bk_gpio_disable_pull(HW_LDO_GPIO);
    bk_gpio_enable_output(HW_LDO_GPIO); 
    bk_gpio_set_output_high(HW_LDO_GPIO);
    LOGI("board power lock GPIO%d=1\r\n", HW_LDO_GPIO);

    gpio_dev_unmap(ALI_OPTICAL_EN_GPIO);
    bk_gpio_disable_pull(ALI_OPTICAL_EN_GPIO);
    bk_gpio_enable_output(ALI_OPTICAL_EN_GPIO);
    bk_gpio_set_output_high(ALI_OPTICAL_EN_GPIO);
    LOGI("board touch optical enable GPIO%d=1\r\n", ALI_OPTICAL_EN_GPIO);

    return 0;

}
static board_module_t g_board = {
    .super.init = _boards_init,
    .getMac = board_get_mac,
    .getSubMac = board_get_sub_mac,
    .getUid = board_get_uuid,
    .startSleep = _boards_enter_deep_sleep,
};

board_module_t *board_instance(void)
{
    return &g_board;
}
