#ifndef __APP_IPC_H__
#define __APP_IPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief IPC command ID enumeration
 * Defines various command types supported by application layer IPC communication
 */
typedef enum{
    IPC_CMD_CAPTIVE_DNS_START,  /**< Start Captive DNS service */
    IPC_CMD_CAPTIVE_DNS_STOP,   /**< Stop Captive DNS service */
} APP_IPC_CMD_E;

/**
 * @brief IPC asynchronous callback function type
 * @param cmd_id Command ID corresponding to the sent command
 * @param result Execution result of the command
 * @param user_data User data passed when sending
 */
typedef void (*app_ipc_async_cb_t)(uint32_t cmd_id, uint32_t result, void *user_data);

/**
 * @brief Initialize IPC module
 * @return BK_OK on success, error code on failure
 */
bk_err_t app_ipc_init(void);

/**
 * @brief Send message synchronously
 * @param cmd_id Command ID, reference @ref APP_IPC_CMD_E
 * @param data Pointer to data to be sent
 * @param len Length of data to be sent
 * @param result Pointer to store execution result
 * @return BK_OK on success, error code on failure
 */
bk_err_t app_ipc_send_sync(uint32_t cmd_id, void *data, uint32_t len, uint32_t *result);

/**
 * @brief Send message asynchronously
 * @param cmd_id Command ID, reference @ref APP_IPC_CMD_E
 * @param data Pointer to data to be sent
 * @param len Length of data to be sent
 * @param cb Callback function to receive execution result
 * @param user_data User data passed to callback function
 * @return BK_OK on success, error code on failure
 */
bk_err_t app_ipc_send_async(uint32_t cmd_id, void *data, uint32_t len, app_ipc_async_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif