#include <components/log.h>
#include "bk_ef.h"

#include "bk_app_asr.h"
#include "app_asr_intf.h"

#define TAG "asr"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define SYS_DATA_KWS_AUTH "kws_auth"

int bk_app_asr_init(void)
{
    const app_asr_intf_t *asr_handle = app_asr_intf_instance();

    if (asr_handle == NULL || asr_handle->open == NULL) {
        LOGE("asr_handle is NULL");
        return -1;
    }

    return asr_handle->open();
}

void bk_app_asr_deinit(void)
{
    const app_asr_intf_t *asr_handle = app_asr_intf_instance();

    if (asr_handle == NULL || asr_handle->close == NULL) {
        LOGE("asr_handle is NULL");
        return;
    }

    asr_handle->close();
}

int bk_app_asr_recog(void *data, uint32_t len, void *p1, void *p2)
{
    const app_asr_intf_t *asr_handle = app_asr_intf_instance();

    if (asr_handle == NULL || asr_handle->process == NULL) {
        LOGE("asr_handle is NULL");
        return -1;
    }

    return asr_handle->process(data, len, p1, p2);
}

int bk_app_asr_get_auth(asr_auth_t *auth)
{
    bk_err_t ret = BK_FAIL;

    do {
        if (!auth) {
            LOGE("param is NULL\r\n");
            break;
        } else {
            ret = bk_get_env_enhance(SYS_DATA_KWS_AUTH, auth, sizeof(asr_auth_t));
            if (ret > 0) {
                LOGI("get kws auth success, auth_id: %s, auth_key: %s\r\n", auth->id, auth->key);
                ret = BK_OK;
            } else {
                LOGE("get kws auth fail, ret: %d\r\n", ret);
                ret = BK_FAIL;
            }
        }
    } while(0);

    return ret;
}

int bk_app_asr_set_auth(asr_auth_t *auth)
{
    bk_err_t ret = BK_FAIL;

    do {
        if (!auth) {
            LOGE("param is NULL\r\n");
            break;
        } else {
            ret = bk_set_env_enhance(SYS_DATA_KWS_AUTH, auth, sizeof(asr_auth_t));
            if (BK_OK == ret) {
                LOGI("set kws auth success, auth_id: %s, auth_key: %s\r\n", auth->id, auth->key);
            } else {
                LOGE("set kws auth fail, ret: %d\r\n", ret);
            }
        }
    } while(0);

    return ret;
}