#include "device_auth.h"

#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include "bk_ef.h"
#include "pbkdf2_sha256.h"

#define TAG "dev_auth"
#define SYS_DATA_DEVICE_SECRET "device_secret"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

static int device_auth_is_valid_secret(const char *secret)
{
    uint16_t len = 0;

    if (secret == NULL) {
        return 0;
    }

    len = os_strlen(secret);
    return (len > 0) && (len < DEVICE_AUTH_SECRET_MAX_LEN);
}

int device_auth_get_secret(char *secret, uint16_t max_len)
{
    int ret;

    if ((secret == NULL) || (max_len == 0)) {
        return BK_FAIL;
    }

    os_memset(secret, 0, max_len);
    ret = bk_get_env_enhance(SYS_DATA_DEVICE_SECRET, secret, max_len);
    if ((ret <= 0) || !device_auth_is_valid_secret(secret)) {
        LOGW("device secret not provisioned ret:%d\r\n", ret);
        os_memset(secret, 0, max_len);
        return BK_FAIL;
    }

    return BK_OK;
}

int device_auth_set_secret(const char *secret)
{
    if (!device_auth_is_valid_secret(secret)) {
        LOGE("invalid device secret\r\n");
        return BK_FAIL;
    }

    if (bk_set_env_enhance(SYS_DATA_DEVICE_SECRET, secret, os_strlen(secret) + 1) != BK_OK) {
        LOGE("save device secret failed\r\n");
        return BK_FAIL;
    }

    LOGI("device secret saved len:%u\r\n", os_strlen(secret));
    return BK_OK;
}

int device_auth_sign_message(const char *message, char *signature_hex, uint16_t max_len)
{
    char secret[DEVICE_AUTH_SECRET_MAX_LEN];
    uint8_t digest[SHA256_DIGESTLEN];
    HMAC_SHA256_CTX hmac;

    if ((message == NULL) || (signature_hex == NULL) ||
        (max_len < (DEVICE_AUTH_SIGNATURE_HEX_LEN + 1)) ||
        (os_strlen(message) == 0)) {
        return BK_FAIL;
    }

    if (device_auth_get_secret(secret, sizeof(secret)) != BK_OK) {
        return BK_FAIL;
    }

    os_memset(digest, 0, sizeof(digest));
    os_memset(&hmac, 0, sizeof(hmac));
    hmac_sha256_init(&hmac, (const uint8_t *)secret, os_strlen(secret));
    hmac_sha256_update(&hmac, (const uint8_t *)message, os_strlen(message));
    hmac_sha256_final(&hmac, digest);

    os_memset(signature_hex, 0, max_len);
    for (uint16_t i = 0; i < SHA256_DIGESTLEN; i++) {
        os_snprintf(signature_hex + (i * 2), max_len - (i * 2), "%02x", digest[i]);
    }

    return BK_OK;
}
