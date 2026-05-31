#ifndef __SYSTEM_MANAGER_H__
#define __SYSTEM_MANAGER_H__

#include "common.h"

#define SYSTEM_MANAGER_QUEUE_SIZE                 10
#define SYSTEM_MANAGER_QUEUE_NAME                 "sys_manager"
#define SYSTEM_MANAGER_TASK_NAME                  "sys_manager"
#define SYSTEM_MANAGER_TASK_PRIORITY              4
#define SYSTEM_MANAGER_TASK_SIZE                  (4096)

typedef enum{
    SYSTEM_STATUS_NULL = 0,

    SYSTEM_STATUS_INIT,
    SYSTEM_STATUS_NET_NULL,                       //等待配网
    SYSTEM_STATUS_NET_CONFIGING,                  //配网中
    SYSTEM_STATUS_NET_DISCONNECT,
    SYSTEM_STATUS_NET_CONFIG_DONE,                //配网完成

    SYSTEM_STATUS_NET_CONNECTING,                  //wifi连接中
    SYSTEM_STATUS_NET_CONNECTED,                   //wifi连接完成
    
    SYSTEM_STATUS_DEV_ACTIVING,                    //设备激活中
    SYSTEM_STATUS_DEV_ACTIVED,                     //设备已激活

    SYSTEM_STATUS_IDLE,                            //空闲

    SYSTEM_STATUS_SERV_NULL,                        //云端断连
    SYSTEM_STATUS_SERV_CONNECT_START,               //云连接开始
    SYSTEM_STATUS_SERV_CONNECTING,                  //云连接中
    SYSTEM_STATUS_SERV_CONNECT_OK,                  //云连接上

    SYSTEM_STATUS_LOCAL_ASR_DIALOG_START,          //离线语音唤醒触发
    SYSTEM_STATUS_LOCAL_ASR_DIALOG_END,            //离线语音唤醒结束

    SYSTEM_STATUS_RECORDING,                       //录音中
    SYSTEM_STATUS_RECORDING_DONE,                  //录音完成
    SYSTEM_STATUS_PLAYING,                         //播放中
    SYSTEM_STATUS_PLAYING_DONE,                    //播放完成
    
}system_status_e;

typedef enum{
    SYSTEM_EVENT_NULL = 0,

    SYSTEM_EVENT_INIT,
    SYSTEM_EVENT_NET_NULL,                       //等待配网
    SYSTEM_EVENT_NET_CONFIGING,                  //配网中
    SYSTEM_EVENT_NET_CONFIG_DONE,                //配网完成
    
    SYSTEM_EVENT_NET_CONNECTING,                 //网络连接中
    SYSTEM_EVENT_NET_CONNECTED,                  //网络连接成功

    SYSTEM_EVENT_NET_CONNECT_FAIL,               //网络连接失败
    SYSTEM_EVENT_NET_PASSWORD_ERR,               //密码错
    SYSTEM_EVENT_NET_NO_AP,                      //没找到热点

    SYSTEM_EVENT_DEV_ACTIVE_START,               //设备开始激活
    SYSTEM_EVENT_DEV_ACTIVE_CODE_DISP,           //设备显示激活码
    SYSTEM_EVENT_DEV_ACTIVE_DONE,                //设备激活成功

    SYSTEM_EVENT_DIALOG_START,                   //设备语音唤醒成功
    
    SYSTEM_EVENT_SERV_CONNECT_START,             //服务器连接中
    SYSTEM_EVENT_SERV_CONNECT_OK,                //服务器连接上
    SYSTEM_EVENT_SERV_NULL,                      //服务器断连

    SYSTEM_EVENT_DIALOG_ABORT,                    //打断
    
    SYSTEM_EVENT_RECORD_START,                    //开始录音
    SYSTEM_EVENT_RECORD_END,                      //录音结束

    SYSTEM_EVENT_ASR_TIMEOUT,                     //对话云端识别超时
    SYSTEM_EVENT_PLAY_START,                      //开始播放
    SYSTEM_EVENT_PLAY_END,                        //播放结束

    SYSTEM_EVENT_LAST_SENTENCE,                   //收到byebye
    SYSTEM_EVENT_DIALOG_END,                        //对话结束
    SYSTEM_EVENT_IDLE,                            //进入空闲状态
    SYSTEM_EVENT_UI_DISP_TEXT,                    //刷新ui文本
    SYSTEM_EVENT_UI_DISP_EMOJI,                   //刷新表情

    SYSTEM_EVENT_OTA_START,                       //开始更新固件
    SYSTEM_EVENT_OTA_FAIL,                        //更新固件失败
    SYSTEM_EVENT_SPK_UP,                          //提高喇叭音量
    SYSTEM_EVENT_SPK_DOWN,                        //减小喇叭音量
    SYSTEM_EVENT_DEV_DEEP_SLEEP,                  //关机
    SYSTEM_EVENT_FACTORY_RESET,                   //恢复出厂设置
}system_event_e;

typedef struct{
    system_event_e event;
    void* param;
}sysMsg_t;

typedef struct{
    super_module_t               super;
    void                         (*send_msg)(sysMsg_t*);
    void                         (*send_msg_by_event)(system_event_e event);
    system_status_e              (*get_system_status)(void);
    char*                        (*get_system_status_string_fmt)(system_status_e system_status);
    void                         (*event_cb)(system_event_e event, void *data);

    bool                         m_is_init;
    beken_queue_t                m_queue;
    beken_thread_t               m_thread_hd;
    beken_semaphore_t            m_sem;
    task_state_e                 m_task_state;
    volatile system_status_e     m_system_status;
    bool                         m_is_last;
}system_manager_module_t;

system_manager_module_t *system_manager_instance(void);
#endif
