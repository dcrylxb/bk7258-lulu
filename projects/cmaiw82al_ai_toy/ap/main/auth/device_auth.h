#ifndef __DEVICE_AUTH_H__
#define __DEVICE_AUTH_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_AUTH_SECRET_MAX_LEN 96
#define DEVICE_AUTH_SIGNATURE_HEX_LEN 64

int device_auth_get_secret(char *secret, uint16_t max_len);
int device_auth_set_secret(const char *secret);
int device_auth_sign_message(const char *message, char *signature_hex, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif
