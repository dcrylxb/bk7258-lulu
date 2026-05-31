#include "bk_api_ipc.h"
#include "os/mem.h"
#include "common/bk_include.h"

#include "app_ipc.h"

#define TAG "app_ipc"

/**
 * @brief IPC消息结构体
 * 用于CPU间通信的消息格式
 */
typedef struct{
    uint32_t cmd;          /**< 命令ID，标识消息类型 */
    uint32_t data;         /**< 用户数据指针 */
    uint32_t len;          /**< 用户数据长度 */
    int32_t ret;           /**< 返回值，异步通信时由IPC框架在tx_cb回调前设置执行结果 */
    app_ipc_async_cb_t cb; /**< 异步回调函数指针 */
    void *user_data;       /**< 用户数据指针，用于回调函数传递用户数据 */
} app_message_t;

BK_IPC_CHANNEL_DEF(g_app_ipc);

static uint32_t app_ipc_rx_callback(uint8_t *data, uint32_t size, void *param, ipc_obj_t ipc_obj)
{
    app_message_t *msg = (app_message_t *)data;
    
    switch(msg->cmd) {
        default:
            break;
    }
    
    return BK_OK;
}

/**
 * @brief IPC异步通信回调函数
 * @note 当异步消息发送完成时，IPC框架会调用此函数
 *
 * @param ipc_obj IPC对象
 *
 * @return BK_OK 成功，其他值表示失败
 */
static uint32_t app_ipc_tx_callback(ipc_obj_t ipc_obj)
{
    uint32_t size = 0;
    app_message_t *msg = (app_message_t *)bk_ipc_obj_convert(ipc_obj, &size);

    if (!msg) {
        return BK_OK;
    }

    if (msg->cb) {
        msg->cb(msg->cmd, msg->ret, msg->user_data);
    }

    os_free(msg);

    return BK_OK;
}

BK_IPC_CHANNEL_REGISTER(g_app_ipc, IPC_ROUTE_CPU0_CPU1, app_ipc_rx_callback, NULL, app_ipc_tx_callback);

int app_ipc_init(void)
{
    return bk_ipc_init();
}

int app_ipc_send_sync(uint32_t cmd_id, void *data, uint32_t len, uint32_t *result)
{
    int ret = 0;
    app_message_t *msg = NULL;
    
    do {
        msg = os_malloc(sizeof(app_message_t));
        if (!msg) {
            ret = BK_ERR_NO_MEM;
            break;
        }
        
        memset(msg, 0, sizeof(app_message_t));
        msg->cmd = cmd_id;
        msg->data = (uint32_t)data;
        msg->len = (uint32_t)len;
        
        ret = bk_ipc_send(&g_app_ipc, (void*)msg, sizeof(app_message_t), 
                         MIPC_CHAN_SEND_FLAG_SYNC, result);
        
        os_free(msg);
    } while(0);
    
    return ret;
}

int app_ipc_send_async(uint32_t cmd_id, void *data, uint32_t len, app_ipc_async_cb_t cb, void *user_data)
{
    int ret = 0;
    app_message_t *msg = NULL;
    
    do {
        msg = os_malloc(sizeof(app_message_t));
        if (!msg) {
            ret = BK_ERR_NO_MEM;
            break;
        }
        
        memset(msg, 0, sizeof(app_message_t));
        msg->cmd = cmd_id;
        msg->data = (uint32_t)data;
        msg->len = (uint32_t)len;
        msg->cb = cb;
        msg->user_data = user_data;
        
        ret = bk_ipc_send(&g_app_ipc, (void*)msg, sizeof(app_message_t), 
                         MIPC_CHAN_SEND_FLAG_DEFAULT, NULL);
        
        // 发送失败时，释放消息内存。成功时，在tx_cb回调中释放
        if (ret != BK_OK) {
            os_free(msg);
        }
    } while(0);
    
    return ret;
}