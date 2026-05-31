#ifndef BK_KWS_H_
#define BK_KWS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"


// #define KWS_TRHRESHOLD 0.98f


#define Feat_M           (16)
#define Feat_N           (32)
#define FFT_LEN          (1024)

typedef struct _MFCC_INFO
{
    uint32_t framecnt;
    uint32_t base;
    int32_t pre_coe;
    int32_t x_old;
    int32_t frame_len;
    int32_t skip;
    int32_t Flen;
    int32_t max_val;
    int32_t FN;
    int32_t frame_max[Feat_N];
    int32_t feat_buff[Feat_N][Feat_M];
    int8_t feat_out[Feat_N][Feat_M];
    int32_t tbuff[FFT_LEN];
    int32_t fbuff[FFT_LEN+4];
    int32_t *spec;
    int32_t *amp;
}MFCC_INFO;

#define KWS_FRAME_SIZE 640
#define MAX_LIST_CNT 12
#define MAX_KWS_CNT 18
typedef struct _KWS_INFO
{
    int32_t frame_cnt;
    int32_t success_cnt;
    int32_t start_flag;
    int32_t score_list[MAX_KWS_CNT][MAX_LIST_CNT];
    int32_t model_id;
    int32_t idx;
    int32_t KN;
    int32_t LN;
    MFCC_INFO mfcc_info;
    void * TF_Hdl;
    uint8_t * tf_buff;
    int16_t * p_mic_data;
    int32_t * p_thr_tab1;
}KWS_INFO;

// extern const unsigned char g_micro_speech_quantized_model_data[];
// extern const unsigned char g_micro_speech_quantized_model_data_B[];
// // extern const unsigned int g_micro_speech_quantized_model_data_len;

extern char* bk_kws_ver(void);
extern uint32_t mfcc_ver(void);
extern void mfcc_init(MFCC_INFO * p_mfcc);
extern int mfcc_calue(MFCC_INFO * p_mfcc, const int16_t * datain);
extern uint32_t rtos_get_time(void);
extern int bk_uart_write_bytes(uint32_t id, const void *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  
