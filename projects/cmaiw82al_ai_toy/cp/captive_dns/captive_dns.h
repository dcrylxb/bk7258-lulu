/**
 * @file captive_dns.h
 * @brief Captive DNS service header file
 */

#ifndef __CAPTIVE_DNS_H__
#define __CAPTIVE_DNS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize captive DNS service
 * @return 0 on success, -1 on failure
 */
int captive_dns_init(void);

/**
 * @brief Deinitialize captive DNS service
 * @return 0 on success, -1 on failure
 */
int captive_dns_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAPTIVE_DNS_H__ */