#include <stdio.h>
#include <string.h>

#include <os/os.h>
#include <common/bk_include.h>
#include "asr.h"
#include "bk_wifi.h"

#include "app_asr_intf.h"

#define TAG  "wanson"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define USER_ID "773691CC577C08DD"  // 从上海华镇电子科技有限公司获取用户ID，需要通获取授权使用的用户ID保持一致

typedef struct {
    bool is_init;
    uint8_t userid[8];

    beken_thread_t auth_task_hdl;
    bool is_running;

#if (CONFIG_WANSON_ASR_GROUP_VERSION)
    Fst fst_1;
    Fst fst_2;
    uint8_t asr_curr_group_id;
#endif
} wanson_asr_ctx_t;

static wanson_asr_ctx_t g_wanson_asr_ctx = {0};

#if (CONFIG_WANSON_ASR_GROUP_VERSION)
/**
 * @brief 分组设置 
 * 
 * 当前设备没有播放音乐时，切换成分组1
 * 当设备在播放音乐时，将其切换成分组2
 * 
 * @param group_id 分组ID
 */
static void wanson_fst_group_change(unsigned char group_id)
{
    if(g_wanson_asr_ctx.asr_curr_group_id != group_id) {
        g_wanson_asr_ctx.asr_curr_group_id = group_id;

        if (group_id == 1) {
            Wanson_ASR_Set_Fst(&g_wanson_asr_ctx.fst_1);
        } else if (group_id == 2) {
            Wanson_ASR_Set_Fst(&g_wanson_asr_ctx.fst_2);
        }
        LOGI("FST group change to: %d\n", group_id);
    }
}

/**
 * @brief 分组初始化
 */
static void wanson_fst_group_init(void)
{
    /* 指令分组初始化 */
    g_wanson_asr_ctx.fst_1.states = fst01_states;
    g_wanson_asr_ctx.fst_1.num_states = fst01_num_states;
    g_wanson_asr_ctx.fst_1.finals = fst01_finals;
    g_wanson_asr_ctx.fst_1.num_finals = fst01_num_finals;
    g_wanson_asr_ctx.fst_1.words = fst01_words;

    g_wanson_asr_ctx.fst_2.states = fst02_states;
    g_wanson_asr_ctx.fst_2.num_states = fst02_num_states;
    g_wanson_asr_ctx.fst_2.finals = fst02_finals;
    g_wanson_asr_ctx.fst_2.num_finals = fst02_num_finals;
    g_wanson_asr_ctx.fst_2.words = fst02_words;

    /* 设置默认分组 */
    wanson_fst_group_change(2);
    LOGI("Wanson FST group init OK\n");
}
#endif

#if (CONFIG_WANSON_CN_LICENSE)
static int _string_to_bytes(const char *hex_str, uint8_t *buf, size_t size)
{
    size_t i = 0;
    uint8_t value = 0;

    if (!hex_str || !buf || size < 8) {
        LOGE("invalid param\n");
        return -1;
    }
    
    memset(buf, 0, size);
    
    for (i = 0; i < strlen(hex_str) && i < 16; i += 2) {
        if (sscanf(hex_str + i, "%2X", &value) != 1) {
            LOGE("convert hex fail at position %d\n", i);
            return -1;
        }
        buf[i / 2] = value;
    }
    
    return 0;
}

static void wanson_license_task_main(void* param)
{
    int ret = 0;
    wanson_asr_ctx_t *ctx = (wanson_asr_ctx_t *)param;
    wifi_linkstate_reason_t info = {0};
    
    LOGI("Wanson license task start\n");
    
    ctx->is_running = true;
    while (ctx->is_running) {
        bk_wifi_sta_get_linkstate_with_reason(&info);
        if (info.state != WIFI_LINKSTATE_STA_GOT_IP) {
            LOGI("Waiting for WiFi to get IP\n");
            rtos_delay_milliseconds(2000);
            continue;
        }

        extern int wanson_authorization(const char* user_id);
        ret = wanson_authorization((char*)USER_ID);
        if (ret != BK_OK) {
            LOGE("Wanson authorization fail, ret: %d\n", ret);
            break;
        }

        LOGI("Wanson authorization success\n");

        // 授权成功后，重新初始化
        ret = Wanson_ASR_Init(ctx->userid);
        if (ret < 0) {
            LOGE("Wanson ASR init fail, ret: %d\n", ret);
            break;
        }
    
        #if (CONFIG_WANSON_ASR_GROUP_VERSION)
        // 初始化分组
        wanson_fst_group_init();
        #else
        Wanson_ASR_Reset();
        #endif

        ctx->is_init = true;
        LOGI("Wanson ASR init success\n");
    }
    
    ctx->auth_task_hdl = NULL;
    rtos_delete_thread(NULL);
}

static void _wanson_license_init(void)
{
    int ret = 0;

    LOGI("Wanson license init\n");

    ret = rtos_create_thread(&g_wanson_asr_ctx.auth_task_hdl,
                             5,
                             "wanson_license",
                             (beken_thread_function_t)wanson_license_task_main,
                             2*1024,
                             (void *)&g_wanson_asr_ctx);
    if (ret != BK_OK) {
        LOGE("create wanson license task fail, ret: %d\n", ret);
        return;
    }
}

static int _wanson_asr_open(void)
{
    int ret = 0;

    memset(&g_wanson_asr_ctx, 0, sizeof(g_wanson_asr_ctx));

    do {
        _string_to_bytes(USER_ID, g_wanson_asr_ctx.userid, sizeof(g_wanson_asr_ctx.userid));

        ret = Wanson_ASR_Init(g_wanson_asr_ctx.userid);
        if (ret < 0) {
            LOGE("Wanson ASR init fail, ret: %d\n", ret);
            break;
        }
        
        #if (CONFIG_WANSON_ASR_GROUP_VERSION)
        // 初始化分组
        wanson_fst_group_init();
        #else
        Wanson_ASR_Reset();
        #endif
    
        g_wanson_asr_ctx.is_init = true;
        LOGI("Wanson ASR init success\n");
    } while (0);
    
    if (!g_wanson_asr_ctx.is_init) {
        // 未授权初始化失败，启动授权任务
        _wanson_license_init();
    }
    
    // 无论初始化成功还是失败，都返回0，保证音频服务正常启动
    return 0;
}
#else
static int _wanson_asr_open(void)
{
    int ret = 0;

    memset(&g_wanson_asr_ctx, 0, sizeof(g_wanson_asr_ctx));

    ret = Wanson_ASR_Init();
    if (ret < 0) {
        LOGE("Wanson ASR init fail, ret: %d\n", ret);
        return ret;
    }
 
#if (CONFIG_WANSON_ASR_GROUP_VERSION)
    // 初始化分组
    wanson_fst_group_init();
#else
    Wanson_ASR_Reset();
#endif

    g_wanson_asr_ctx.is_init = true;
    LOGI("Wanson ASR init success\n");

    return 0;
}
#endif

static void _wanson_asr_close(void)
{
    g_wanson_asr_ctx.is_running = false;

    if (g_wanson_asr_ctx.is_init) {
        Wanson_ASR_Release();
    }

    LOGI("Wanson ASR close success\n");
}

static int _wanson_asr_process(void *data, uint32_t len, void *p1, void *p2)
{
    if (!g_wanson_asr_ctx.is_init) {
        return -1;
    }
    
    return Wanson_ASR_Recog((short *)data, len>>1, p1, p2); // 转成 short 类型，长度减半
}

const app_asr_intf_t *wanson_asr_instance(void)
{
    static const app_asr_intf_t wanson_asr_intf = {
        .name    = "wanson",
        .open    = _wanson_asr_open,
        .close   = _wanson_asr_close,
        .process = _wanson_asr_process,
    };
    
    return &wanson_asr_intf;
}