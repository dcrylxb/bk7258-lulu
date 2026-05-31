#include "app_asr_intf.h"

const app_asr_intf_t *app_asr_intf_instance(void)
{
#if CONFIG_WANSON_ARMINO_ASR
    extern const app_asr_intf_t *wanson_asr_instance(void);
    return wanson_asr_instance();
#elif CONFIG_BEKEN_KWS
    extern const app_asr_intf_t *kws_asr_instance(void);
    return kws_asr_instance();
#else
    return NULL;
#endif
}