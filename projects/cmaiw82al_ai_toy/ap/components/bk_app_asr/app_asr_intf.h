#ifndef __APP_ASR_INTF__H__
#define __APP_ASR_INTF__H__

#include <common/bk_include.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    int  (*open)(void);
    void (*close)(void);
    int  (*process)(void *data, uint32_t len, void *p1, void *p2);
} app_asr_intf_t;

const app_asr_intf_t *app_asr_intf_instance(void);

#if __cplusplus
}
#endif
#endif