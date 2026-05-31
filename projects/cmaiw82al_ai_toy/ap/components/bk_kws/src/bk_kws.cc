
#include <algorithm>
#include <cstdint>
#include <iterator>

#include "bk_kws.h"
#include "os/os.h"
#include "os/mem.h"
#include <string.h>

namespace Bk_KWS{
KWS_INFO kws_info;
KWS_INFO * p_kws = &kws_info;
#define TFLM_BUFF_SIZE (40 * 1024)
uint32_t *kws_tflm_buffer = NULL;
#define KWS_TFLM_BUFFSIZE  (38*1024)

#define KWS_MODEL_SWITCH   (0)

#define IDX_OFFSET    (1)   // 1
#define KWS_DIAG_LOW_THRESHOLD  20000
///////////////////////////////////////////////////////

#if 1
// XZ  XA  XL  WAKE UP
const int KWS_POST_LIST_TAB[] = {8,8};
const int KWS_POST_A_TAB[]={KWS_DIAG_LOW_THRESHOLD,KWS_DIAG_LOW_THRESHOLD};
#endif

#if KWS_MODEL_SWITCH
const int KWS_POST_B_TAB[]={65000*5,65000*3};
#endif
///////////////////////////////////////////////////////
#if 1   // for XZ  XL wake up
constexpr const char *kCategoryLabels[MAX_KWS_CNT] =
{
    "unknown",
    "hello",
    "byebye",
};
#endif

#if 0  // for india tudu wake up and cmds
constexpr const char *kCategoryLabels[MAX_KWS_CNT] =
{
    "unknown",
    "heytodu",
    "play",
    "stop",
    "next",
    "incre",
    "decre",
};
#endif

#if 0   // for XA wake up and cmds
constexpr const char *kCategoryLabels[MAX_KWS_CNT] =
{
    "unknown",
    "xiaoai",  //1
    "play_music", //2
    "stop_playing", //3
    "photo_capture", //4
    "receive_call", //5
    "start_video", //6
    "start_camera",  //7
    "incre_volume",  //8
    "decre_volume", //9
    "last_music", //10
    "next_music", //11
    "noise",      //12
    "NOISE",      //13
};
#endif


#if 0
const int KWS_POST_LIST_TAB[] = {8,8};
const int KWS_POST_A_TAB[]={65000*0,65000*5};
constexpr const char *kCategoryLabels[MAX_KWS_CNT] =
{
    "hello",
};
#endif

typedef enum {
	KWS_MODEL_WAKEUP = 0,
	KWS_MODEL_CMDS,
	KWS_MODEL_RSV,
} KWS_MODED_TYPE;


#define KWS_DUMP_DATA 0
#define KWS_PCM_DIAG_PERIOD_FRAMES 250
static const char *KWS_PCM_DIAG_FMT = "kws pcm diag frame=%d avg_abs=%d min=%d max=%d len=%d\r\n";
#define KWS_SCORE_DIAG_PERIOD_FRAMES 250
static const char *KWS_SCORE_DIAG_FMT = "kws score diag frame=%d raw0=%d raw1=%d raw2=%d score0=%d score1=%d noise_gate=%d gate=%d thr=%d best=%d\r\n";
#define KWS_SCORE_PEAK_DIAG_PERIOD_FRAMES 25
static const char *KWS_SCORE_PEAK_DIAG_FMT = "kws score peak diag frame=%d peak_gate=%d peak_score0=%d peak_score1=%d peak_noise_gate=%d peak_best=%d thr=%d\r\n";
static int s_peak_gate_score = 0;
static int s_peak_score0 = 0;
static int s_peak_score1 = 0;
static int s_peak_score2 = 0;
static int s_peak_best_id = -1;

#if KWS_DUMP_DATA
static int dump_flag=0;
static int micval=0;
static int dcnt1=0;
static int dcnt2=0;
void data_dump(int16_t *p0,int32_t size)
{
	int32_t len = size>>1;
  int16_t *p1 = p0;
  int32_t k=0;

  dcnt1++;
  micval = 0;
  for(k=0;k<len;k++)
  {
    if(p1[k]>0)
    {
      micval += p1[k];
    }
    else
    {
      micval -= p1[k];
    } 
  }
  micval /= len;

  if(micval>22000)
  {
    if ( (dcnt1-dcnt2)>50 )
    {
       dcnt2=dcnt1;
      dump_flag = !dump_flag;
    }
  }
  if(dump_flag)
	{
    bk_uart_write_bytes(1, (const int16_t*)p1, size);
	}
}




#endif

////////////////////////
extern "C" void* tflm_model_get_for_kws_model(void);
extern "C" void* tflm_resolver_get_for_kws_model(void);
extern "C" char* tflm_version_get_for_kws_model(void);
extern "C" uint32_t kws_auth_ver(void);

extern "C" void* tflm_model_get_for_kws_model2(void);
extern "C" void* tflm_resolver_get_for_kws_model2(void);
extern "C" char* tflm_version_get_for_kws_model2(void);

extern "C" int tflm_interpreter_init(void** handle, uint8_t* model, void* resolver, uint32_t tensor_arena_size);
extern "C" int tflm_interpreter_init2(void** handle, uint8_t* model, void* resolver, uint8_t* tensor_arena_buff, uint32_t tensor_arena_size);
extern "C" int tflm_interpreter_deinit(void* handle);
extern "C" void* tflm_interpreter_run(void* handle, int8_t* data);
extern "C" void* tflm_interpreter_output(void* handle, uint32_t index, uint32_t* size);

///////////////////////////////////////
#if 0
static void adc_gain_set(int ch, uint32_t gain)
{
#define ADC1_GAIN_ADD    (0x4401014c)
#define ADC2_GAIN_ADD    (0x4401016c)
#define ANA_GAIN_MSK     (0x0f<<15)
   uint32_t ANA_GAIN_REG = gain<<15;
   uint32_t reg1 = (*((volatile UINT32 *)(ADC1_GAIN_ADD)));
   uint32_t reg2 = (*((volatile UINT32 *)(ADC2_GAIN_ADD)));
   uint32_t val1 = reg1 & ANA_GAIN_MSK;
   uint32_t val2 = reg2 & ANA_GAIN_MSK;
   reg1 &= ~(ANA_GAIN_MSK);
   reg1 |= ANA_GAIN_REG;
   reg2 &= ~(ANA_GAIN_MSK);
   reg2 |= ANA_GAIN_REG;

   if(val1 !=ANA_GAIN_REG)
   {
      (*((volatile UINT32 *)(ADC1_GAIN_ADD))) = reg1;
       os_printf("@#@# set adc1 ana gain %x  %d\n",reg1,gain);
   }

   if(ch>1)
   {
      if(val2 != ANA_GAIN_REG)
      {
         (*((volatile UINT32 *)(ADC2_GAIN_ADD))) = reg2;
          os_printf("@#@# set adc2 ana gain %x  %d\n",reg2,gain);
      }
   }
}
#endif

int GenerateFeatures(const int16_t* audio_data,
                              int8_t **out_data) 
{
  const int16_t *p=audio_data;
  
  int ret = mfcc_calue(&p_kws->mfcc_info, p);
  (void)ret;
  *out_data = &p_kws->mfcc_info.feat_out[0][0];

  return 0;
}

#if KWS_MODEL_SWITCH
extern "C" int tflm_model_switch(int id)
{
  int err = 0;
  if(id != p_kws->model_id)
  {
     int32_t * post_ptr=NULL;
     uint8_t * pm = NULL;
     void    * pr = NULL;
     char    * pv = NULL;
     int32_t   LN = 0;
     switch (id)
     {
        case KWS_MODEL_WAKEUP:
             post_ptr = (int32_t*)KWS_POST_A_TAB;
             pm = (uint8_t*)tflm_model_get_for_kws_model();
             pr = tflm_resolver_get_for_kws_model();
             pv = tflm_version_get_for_kws_model();
             LN = KWS_POST_LIST_TAB[0];
             break;
        case KWS_MODEL_CMDS:
        default:
             post_ptr = (int32_t*)KWS_POST_B_TAB;
             pm = (uint8_t*)tflm_model_get_for_kws_model2();
             pr = tflm_resolver_get_for_kws_model2();
             pv = tflm_version_get_for_kws_model2();
             LN = KWS_POST_LIST_TAB[1];
             break;
     }
     ///////////  backup before memset
     uint32_t  base = p_kws->mfcc_info.base;
     uint8_t * tf_buff_p = p_kws->tf_buff;
     ///////////
     err = tflm_interpreter_deinit(p_kws->TF_Hdl);
     os_printf("tflm [%d] deninit %d\n",p_kws->model_id,err);
     memset(p_kws,0,sizeof(KWS_INFO));
     mfcc_init(&p_kws->mfcc_info);
     p_kws->mfcc_info.FN = 32;
     p_kws->p_thr_tab1 = (int32_t*)post_ptr;
     p_kws->mfcc_info.base = base;
     p_kws->p_mic_data = (int16_t*)p_kws->mfcc_info.fbuff; //reuse
     p_kws->tf_buff = tf_buff_p;
     err = tflm_interpreter_init2(&p_kws->TF_Hdl, pm, pr, p_kws->tf_buff, KWS_TFLM_BUFFSIZE);
     uint32_t out_N = 0;
     tflm_interpreter_output(p_kws->TF_Hdl, 0, &out_N);
     p_kws->KN = out_N-1;
     p_kws->LN = LN;
     p_kws->model_id = id;
     os_printf("tflm_init %d  KN %d LN %d ver %s\r\n",err,p_kws->KN,p_kws->LN,pv);
  }
  else
  {
     os_printf("same model id\n");
  }
  return err;
}
#endif

void tflite_recgonize(int16_t *audio_data,uint32_t audio_data_size, const char **text, float *score, int16_t *result)
{
  int8_t *outdata=NULL;
  int8_t *feature=NULL;

  GenerateFeatures(audio_data, &feature);


  outdata = (int8_t*)tflm_interpreter_run(p_kws->TF_Hdl, feature);
  
  *result = 0;

  p_kws->frame_cnt++;

  #if KWS_DUMP_DATA
  if(dump_flag==0)
  os_printf("## result1 %d, result2 %d  micval %d ##\r\n",outdata[1], outdata[2],micval);
  #endif

  int m,n,val,id = p_kws->idx,thr=p_kws->p_thr_tab1[1];
  int raw0 = outdata[0];
  int raw1 = (p_kws->KN > 0) ? outdata[IDX_OFFSET] : 0;
  int raw2 = (p_kws->KN > 1) ? outdata[IDX_OFFSET + 1] : 0;
  int score0 = 0;
  int score1 = 0;
  int score2 = 0;
  int gate_score = 0;
  int best_id = -1;

  for (m=0;m<p_kws->KN;m++)
  {
    val = (outdata[m+IDX_OFFSET]+128);
    ////val = (val>KWS_POST_A_THR_TAB[m])?256:0;
    val = val*val;
    p_kws->score_list[m][id]=val;
  }

//////////////////////////
  val = 256-(outdata[0]+128);  ///// noise val
  p_kws->score_list[m][id]=val*val;
  val=0;
  for(n=0;n<p_kws->LN;n++)
  {
     val += p_kws->score_list[m][n];
  }
  gate_score = val;
//////////////////////////
  if(++id==p_kws->LN) id=0;
  p_kws->idx=id;

  id = -1;
  if(val>p_kws->p_thr_tab1[0])
  {
    for (m=0;m<p_kws->KN;m++)
    {
      val=0;
      for(n=0;n<p_kws->LN;n++)
      {
         val += p_kws->score_list[m][n];
      }
  
      if(val>thr)
      {
        id=m;
        thr = val;
        best_id = m;
        *text = kCategoryLabels[m+IDX_OFFSET];
      }
    }
  }

  if (p_kws->KN > 0)
  {
    for(n=0;n<p_kws->LN;n++)
    {
      score0 += p_kws->score_list[0][n];
    }
  }
  if (p_kws->KN > 1)
  {
    for(n=0;n<p_kws->LN;n++)
    {
      score1 += p_kws->score_list[1][n];
    }
  }
  for(n=0;n<p_kws->LN;n++)
  {
    score2 += p_kws->score_list[p_kws->KN][n];
  }

  int current_best_id = -1;
  int current_best_score = p_kws->p_thr_tab1[1];
  if (score0 > current_best_score)
  {
    current_best_score = score0;
    current_best_id = 0;
  }
  if (score1 > current_best_score)
  {
    current_best_score = score1;
    current_best_id = 1;
  }
  if (gate_score > s_peak_gate_score)
  {
    s_peak_gate_score = gate_score;
  }
  if (score0 > s_peak_score0)
  {
    s_peak_score0 = score0;
  }
  if (score1 > s_peak_score1)
  {
    s_peak_score1 = score1;
  }
  if (score2 > s_peak_score2)
  {
    s_peak_score2 = score2;
  }
  if (current_best_id >= 0)
  {
    s_peak_best_id = current_best_id;
  }

  if (p_kws->frame_cnt > 0 && (p_kws->frame_cnt % KWS_SCORE_PEAK_DIAG_PERIOD_FRAMES) == 0)
  {
    os_printf(KWS_SCORE_PEAK_DIAG_FMT, p_kws->frame_cnt, s_peak_gate_score, s_peak_score0, s_peak_score1, s_peak_score2, s_peak_best_id, p_kws->p_thr_tab1[1]);
    s_peak_gate_score = 0;
    s_peak_score0 = 0;
    s_peak_score1 = 0;
    s_peak_score2 = 0;
    s_peak_best_id = -1;
  }

  if (p_kws->frame_cnt > 0 && (p_kws->frame_cnt % KWS_SCORE_DIAG_PERIOD_FRAMES) == 0)
  {
    if (best_id < 0 && current_best_id >= 0)
    {
      best_id = current_best_id;
    }
    os_printf(KWS_SCORE_DIAG_FMT, p_kws->frame_cnt, raw0, raw1, raw2, score0, score1, score2, gate_score, p_kws->p_thr_tab1[1], best_id);
  }

 //////////////////////////
  if (id>=0)
  {
     if(((p_kws->frame_cnt-p_kws->start_flag)>30))
     {
        p_kws->start_flag = p_kws->frame_cnt;
		*result = id + IDX_OFFSET;
        os_printf("Wakeup  %d  %s  %d\r\n",id,*text,++p_kws->success_cnt);
        
        ////////////////////
        #if KWS_MODEL_SWITCH
        if(p_kws->model_id==KWS_MODEL_WAKEUP)
        {
            tflm_model_switch(KWS_MODEL_CMDS);
        }
        else
        {
            tflm_model_switch(KWS_MODEL_WAKEUP);
        }
        #endif
        ////////////////////
     }
  }
}

extern "C" void bk_kws_init(uint32_t base){
    os_printf("kws init %s\n",bk_kws_ver());
    os_printf("kws diag low threshold active: %d\r\n", KWS_DIAG_LOW_THRESHOLD);
    memset(p_kws,0,sizeof(KWS_INFO));
    mfcc_init(&p_kws->mfcc_info);
    p_kws->mfcc_info.FN = 32;
    p_kws->p_thr_tab1 = (int32_t*)KWS_POST_A_TAB;
	p_kws->mfcc_info.base = base;
    p_kws->p_mic_data = (int16_t*)p_kws->mfcc_info.fbuff; //reuse
  // ÎªTensorFlow LiteÄ£ÐÍ·ÖÅäPSRAMÄÚ´æ
  	kws_tflm_buffer = (uint32_t*)psram_malloc(TFLM_BUFF_SIZE);
  	if (kws_tflm_buffer == NULL) {
    	os_printf("PSRAM malloc failed for TFLM buffer, size: %d bytes\n", TFLM_BUFF_SIZE);
    	return;
  	}
  
  // ³õÊ¼»¯TensorFlow Lite½âÊÍÆ÷
  // Ê¹ÓÃtflm_interpreter_init2°æ±¾£¬ÔÊÐí´«Èë×Ô¶¨Òå»º³åÇø
  int err = tflm_interpreter_init2(&p_kws->TF_Hdl, (uint8_t*)tflm_model_get_for_kws_model(), tflm_resolver_get_for_kws_model(), (uint8_t*)kws_tflm_buffer, KWS_TFLM_BUFFSIZE);
    os_printf("tflm_interpreter_init return %d\n",err);
    p_kws->tf_buff = (uint8_t*)kws_tflm_buffer; // backup 
    uint32_t out_N = 0;
    tflm_interpreter_output(p_kws->TF_Hdl, 0, &out_N);
    p_kws->KN = out_N-IDX_OFFSET;
    p_kws->LN = KWS_POST_LIST_TAB[0];
    os_printf("tflm_init %d  KN %d  LN %d ver %s, %u,%u\r\n",err,p_kws->KN,p_kws->LN,tflm_version_get_for_kws_model(),mfcc_ver(),kws_auth_ver());
    // adc_gain_set(1,8);
}

static void kws_pcm_diag(const int16_t *buf, int buf_len)
{
    if (!buf || buf_len <= 0 || p_kws->frame_cnt == 0 ||
        (p_kws->frame_cnt % KWS_PCM_DIAG_PERIOD_FRAMES) != 0) {
        return;
    }

    int samples = buf_len >> 1;
    int32_t min_val = 32767;
    int32_t max_val = -32768;
    int64_t sum_abs = 0;

    for (int i = 0; i < samples; ++i) {
        int32_t v = buf[i];
        if (v < min_val) {
            min_val = v;
        }
        if (v > max_val) {
            max_val = v;
        }
        sum_abs += (v < 0) ? -v : v;
    }

    int32_t avg_abs = samples > 0 ? (int32_t)(sum_abs / samples) : 0;
    os_printf(KWS_PCM_DIAG_FMT, p_kws->frame_cnt, avg_abs, min_val, max_val, samples);
}


extern "C" int  bk_tflite_ASR_Recog(short *buf, int buf_len, const char **text, float *score,int16_t *result)
{
    if(buf_len == KWS_FRAME_SIZE*2)
    {
        memcpy(p_kws->p_mic_data,buf,buf_len);

        #if KWS_DUMP_DATA

          data_dump(p_kws->p_mic_data,(KWS_FRAME_SIZE<<1));

        #endif
        tflite_recgonize(p_kws->p_mic_data,(KWS_FRAME_SIZE<<1),text, score, result);
        kws_pcm_diag(p_kws->p_mic_data, buf_len);
    }
	else
	{
		os_printf("ksw frame len mismatch\r\n");
	}
    
    return 0;
}

 }  // namespace
