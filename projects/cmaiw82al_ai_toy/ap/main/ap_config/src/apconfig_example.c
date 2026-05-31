#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>

#include <modules/wifi.h>
#include <components/event.h>
#include <components/netif.h>
#include <string.h>
#include <components/log.h>

#include "webnet.h"
#include "wn_module.h"
#include "apconfig_example.h"
#include "system_manager.h"
#include "app_ipc.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/prot/dns.h"
#include "lwip/udp.h"

#define TAG "ap_config"


beken_thread_t wifi_cfg_thread;
beken_queue_t  wifi_notify_que;

#define CONFIG_EXAMPLE_DNS  "192.168.188.1"

#define HEADER_GET      "<!doctype html><html lang='Zh'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'><title>WIFI配置</title><style type='text/css'>*,*::before,*::after {box-sizing: border-box;}html {line-height: 1.15;-webkit-text-size-adjust: 100%;}body {margin: 0;font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI',Arial;line-height: 1.5;background-color: #f5f5f5;padding-bottom: 200px;align-items: center;-ms-flex-align: center;display: flex;}p {margin-top: 0;margin-bottom: 1rem;}button{cursor: pointer;}.h3 {margin-top: 0;margin-bottom: 0.5rem;font-weight: 500;line-height: 1.2;font-size: 1.75rem;}.form-control {width: 100%;height: calc(1.5em + 0.75rem + 2px);padding: 0.375rem 0.75rem;font-size: 1rem;font-weight: 400;line-height: 1.5;color: #495057;background-color: #fff;background-clip: padding-box;border: 1px solid #ced4da;border-radius: 0.25rem;transition: border-color 0.15s ease-in-out, box-shadow 0.15s ease-in-out;}.form-control::-ms-expand {background-color: transparent;border: 0;}.form-control:focus {color: #495057;background-color: #fff;border-color: #80bdff;outline: 0;box-shadow: 0 0 0 0.2rem rgba(0, 123, 255, 0.25);}.form-control::-webkit-input-placeholder {color: #6c757d;opacity: 1;}.form-control::-moz-placeholder {color: #6c757d;opacity: 1;}.form-control:-ms-input-placeholder {color: #6c757d;opacity: 1;}.form-control::-ms-input-placeholder {color: #6c757d;opacity: 1;}.form-control:disabled {background-color: #e9ecef;opacity: 1;}.btn {color: #fff;background-color: #007bff;border-color: #007bff;padding: 0.5rem 1rem;font-size: 1.25rem;line-height: 1.5;border-radius: 0.3rem;display: block;width: 100%;}.mb-3 {margin-bottom: 1rem !important;}.mb-4 {margin-bottom: 1.5rem !important;}.text-center {text-align: center !important;}.font-weight-normal {font-weight: 400 !important;}html,body {height: 100%;}.form-signin {width: 100%;max-width: 420px;padding: 15px;margin: auto;}.form-label-group {position: relative;margin-bottom: 1rem;}.form-label-group > input,.form-label-group > label {height: 3.125rem;padding: .75rem;}.form-label-group > label {position: absolute;top: 0;left: 0;display: block;width: 100%;margin-bottom: 0;line-height: 1.5;color: #495057;pointer-events: none;cursor: text;border: 1px solid transparent;border-radius: .25rem;transition: all .1s ease-in-out;}.form-label-group input::-webkit-input-placeholder {color: transparent;}.form-label-group input:-ms-input-placeholder {color: transparent;}.form-label-group input::-ms-input-placeholder {color: transparent;}.form-label-group input::-moz-placeholder {color: transparent;}.form-label-group input::placeholder {color: transparent;}.form-label-group input:not(:placeholder-shown) {padding-top: 1.25rem;padding-bottom: .25rem;}.form-label-group input:not(:placeholder-shown) ~ label {padding-top: .25rem;padding-bottom: .25rem;font-size: 12px;color: #777;}@supports (-ms-ime-align: auto) {.form-label-group > label {display: none;}.form-label-group input::-ms-input-placeholder {color: #777;}}@media all and (-ms-high-contrast: none), (-ms-high-contrast: active) {.form-label-group > label {display: none;}.form-label-group input:-ms-input-placeholder {color: #777;}}</style></head>"
#define BODY_GET        "<body><form method='post' action='/' class='form-signin'><div class='text-center mb-4'><h1 class='h3 mb-3 font-weight-normal'>WIFI配置</h1></div><p>1.wifi名字不能为中文.<br> 2.wifi密码为空可以不填.</p><div class='form-label-group'><input type='text' name='ssid' id='ssid' class='form-control' placeholder='wifi名称' required autofocus><label for='inputEmail'>wifi名称</label></div><div class='form-label-group'><input type='text' id='password' name='password' class='form-control' placeholder='wifi密码'><label for='inputPassword'>wifi密码</label></div><button class='btn' type='submit'>确定</button></form></body></html>\r\n"
#define BODY_POST       "<body><form method='post' action='/' class='form-signin'><div class='text-center mb-4'><h1 class='h3 mb-3 font-weight-normal'>WIFI配置</h1></div><p align='center'>已收到配网信息<br> 请查看板子LCD显示的配网结果。</p><div class='form-label-group'></div><div class='form-label-group'></div></form></body></html>\r\n"
char wifi_get_ssid[33];
char wifi_get_pwd[65];
static net_info_t net_info = {0};
uint8_t g_captive_start_flag = 0;

struct interface
{
    struct netif *netif;
    ip_addr_t ipaddr;
    ip_addr_t nmask;
    ip_addr_t gw;
};


int wifi_send_notify(note_msg_t *arg)
{
	int ret = 0;
	note_msg_t *msg= arg;
	if(wifi_notify_que != NULL)
	{
		ret=rtos_push_to_queue(&wifi_notify_que, (void *)msg, BEKEN_WAIT_FOREVER);
		if(ret != BK_OK)
		{
			bk_printf("wifi queue send notify fial \n");
		}
	}
	return ret;
}

static int wifi_softap_event_cb(void *arg, event_module_t event_module,
					  int event_id, void *event_data)
{
	
	wifi_event_ap_disconnected_t *ap_disconnected;
	wifi_event_ap_connected_t *ap_connected;

	wifi_event_sta_disconnected_t *sta_disconnected;
	wifi_event_sta_connected_t *sta_connected;

	switch (event_id) {
	case EVENT_WIFI_AP_CONNECTED:
		ap_connected = (wifi_event_ap_connected_t *)event_data;
		BK_LOGI(TAG, BK_MAC_FORMAT" connected to AP\n", BK_MAC_STR(ap_connected->mac));

		break;

	case EVENT_WIFI_AP_DISCONNECTED:
		ap_disconnected = (wifi_event_ap_disconnected_t *)event_data;
		BK_LOGI(TAG, BK_MAC_FORMAT" disconnected from AP\n", BK_MAC_STR(ap_disconnected->mac));
		break;

	case EVENT_WIFI_STA_CONNECTED:
		sta_connected = (wifi_event_sta_connected_t *)event_data;
		BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
		break;

	case EVENT_WIFI_STA_DISCONNECTED:
		sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
		BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
		break;

	default:
		BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
		break;
	}

	return BK_OK;
}


static int wifi_netif_event_cb(void *arg, event_module_t event_module,
					   int event_id, void *event_data)
{
	netif_event_got_ip4_t *got_ip;

	switch (event_id) {
	case EVENT_NETIF_GOT_IP4:
		got_ip = (netif_event_got_ip4_t *)event_data;
		BK_LOGI(TAG, "%s got ip\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif");
		break;
	default:
		BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
		break;
	}

	return BK_OK;
}
static void wifi_event_handler_init(void)
{
	//BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, wifi_softap_event_cb, NULL));
	//BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, wifi_netif_event_cb, NULL));
}
static void wifi_event_handler_deinit(void)
{
	//BK_LOG_ON_ERR(bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, wifi_softap_event_cb));
	//BK_LOG_ON_ERR(bk_event_unregister_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, wifi_netif_event_cb));
}

static void wifi_softap_init(void)
{
	bk_wifi_sta_stop();
	wifi_ap_config_t ap_config = WIFI_DEFAULT_AP_CONFIG();

	strncpy(ap_config.ssid, BK_DEFAULT_SSID, WIFI_SSID_STR_LEN);
    ap_config.security = WIFI_SECURITY_NONE;
	//strncpy(ap_config.password, BK_DEFAULT_PASS, WIFI_PASSWORD_LEN);

	BK_LOGI(TAG, "ssid:%s  key:%s\r\n", ap_config.ssid, ap_config.password);
	// bk_wifi_ap_set_config 接口内部设置 AP 的 IP 为 192.168.188.1
	BK_LOG_ON_ERR(bk_wifi_ap_set_config(&ap_config));
	BK_LOG_ON_ERR(bk_wifi_ap_start());
}


static void wifi_sta_init(void)
{
	wifi_sta_config_t sta_config = WIFI_DEFAULT_STA_CONFIG();

	strncpy(sta_config.ssid, wifi_get_ssid, WIFI_SSID_STR_LEN);
	strncpy(sta_config.password, wifi_get_pwd, WIFI_PASSWORD_LEN);

	BK_LOGI(TAG, "ssid:%s password:%s\n", sta_config.ssid, sta_config.password);
	BK_LOG_ON_ERR(bk_wifi_sta_set_config(&sta_config));
	BK_LOG_ON_ERR(bk_wifi_sta_start());
}

static void _wifi_scan_dedup_ssid(wifi_scan_result_t *in, wifi_scan_result_t *out, int size)
{
    int i = 0, j = 0;
    bool found = false;
    int count = 0;

    if (!in || !out || size <= 0) {
        BK_LOGE(TAG, "invalid param\n");
        return;
    }

    for (i = 0; i < in->ap_num && count < size; i++) {
        found = false;

        if (os_strlen(in->aps[i].ssid) == 0) {
            continue;
        }

        for (j = 0; j < count; j++) {
            if (os_strcmp(in->aps[i].ssid, out->aps[j].ssid) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            os_memcpy(&out->aps[count], &in->aps[i], sizeof(wifi_scan_ap_info_t));
            count++;
        }
    }

    out->ap_num = count;
}

static wifi_scan_result_t g_scan_result = {0};
static int _wifi_scan_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_scan_result_t scan_result = {0};

    bk_wifi_scan_get_result(&scan_result);
    //BK_LOG_ON_ERR(bk_wifi_scan_dump_result(&scan_result)); //打印扫描结果

    if(scan_result.ap_num > 0)
    {
        if(!g_scan_result.aps)
        {
            os_printf("[%s]scan cb malloc !\r\n", __func__);
            g_scan_result.aps = (wifi_scan_ap_info_t *)os_malloc(MAX_WIFI_SCAN_AP_NUM*sizeof(wifi_scan_ap_info_t));
        }
        
        if(g_scan_result.aps)
        {
            bk_printf("[%s] get scan result:%d\r\n", __func__, scan_result.ap_num);
            os_memset(g_scan_result.aps, 0, MAX_WIFI_SCAN_AP_NUM*sizeof(wifi_scan_ap_info_t));
            _wifi_scan_dedup_ssid(&scan_result, &g_scan_result, MAX_WIFI_SCAN_AP_NUM);
        }
    }
    
    bk_wifi_scan_free_result(&scan_result);

    return BK_OK;
}

static void cgi_web_ap_config_handler(struct webnet_session* session)
{
	const char* mimetype;
    struct webnet_request* request;
	BK_ASSERT(session !=NULL);
	request = session->request;
	BK_ASSERT(request != NULL);
	bk_printf("%s session:%x\r\n",__FUNCTION__, session);
	
	mimetype = mime_get_type(".html");
    session->request->result_code = 200;
    webnet_session_set_header(session, mimetype, 200, "Ok", -1);
    
	if(request->method == WEBNET_GET)
	{
		bk_printf("WEBNET_GET:%s\r\n", request->session->buffer);
        #if 0
		webnet_session_write(session, (const uint8_t*)HEADER_GET, strlen(HEADER_GET));
		webnet_session_write(session, (const uint8_t*)BODY_GET, strlen(BODY_GET));
        #else
        webnet_session_write(session, (const uint8_t*)PAGE_HEADER, strlen(PAGE_HEADER));
		webnet_session_write(session, (const uint8_t*)PAGE_BODY_GET, strlen(PAGE_BODY_GET));
        #endif
	}
	else if(request->method == WEBNET_POST)
	{
		bk_printf("WEBNET_POST\r\n");
		webnet_session_write(session, (const uint8_t*)HEADER_GET, strlen(HEADER_GET));
		webnet_session_write(session, (const uint8_t*)BODY_POST, strlen(BODY_POST));
		bk_printf("WEBNET_POST111\r\n");
	}
	if (request->query_counter)
    {
        sysMsg_t s_msg = {0};

        const char *ssid_value, *password_value;
        ssid_value = webnet_request_get_query(request, "ssid");
        password_value = webnet_request_get_query(request, "password");

        if(ssid_value)
        {
            memcpy(net_info.ssid,ssid_value,strlen(ssid_value)+1);
        }
        else
        {
            memset(net_info.ssid, 0, sizeof(net_info.ssid));
        }
        
        if(password_value && strlen(password_value))
        {
            memcpy(net_info.pwd,password_value,strlen(password_value)+1);
        }
        else
        {
            memset(net_info.pwd, 0, sizeof(net_info.pwd));
        }
        
        bk_printf("ssid:%s,password:%s\r\n",net_info.ssid,net_info.pwd);
		s_msg.event = SYSTEM_EVENT_NET_CONFIGING;
        s_msg.param = &net_info;
        system_manager_instance()->send_msg(&s_msg);
    }

    return;

}

static void _scan_free_result(void)
{
    if(g_scan_result.aps)
    {
        os_free(g_scan_result.aps);
        g_scan_result.aps = NULL;
        g_scan_result.ap_num = 0;
    }
}
static void cgi_web_scan_handler(struct webnet_session* session)
{
	const char* mimetype;
	struct webnet_request* request;
	char *res_scan_buf = NULL;
	uint32 cur_time = rtos_get_time();
		
	BK_ASSERT(session !=NULL);
	request = session->request;
	BK_ASSERT(request != NULL);

	bk_printf("%s session:%x\r\n",__FUNCTION__, session);

	mimetype = mime_get_type(".html");
	session->request->result_code = 200;
	webnet_session_set_header(session, mimetype, 200, "Ok", -1);

	res_scan_buf = psram_malloc(SCAN_BUFF_SIZE);
	if(!res_scan_buf)
	{
		bk_printf("[%s][%d] malloc mail\r\n", __FUNCTION__, __LINE__);
		return ;
	}
	
	if(request->method == WEBNET_GET)
	{
		wifi_scan_config_t config = {0};

		_scan_free_result();
		
		bk_wifi_scan_start(&config);
		while(1)
		{
			if((rtos_get_time() - cur_time) > SCAN_OVER_TIME)
			{
				bk_printf("[%s][%d] scan over time\r\n", __FUNCTION__, __LINE__);
				bk_wifi_scan_stop();
			}
			
			if(g_scan_result.ap_num > 0 && g_scan_result.aps)
			{
				os_memset(res_scan_buf, 0, SCAN_BUFF_SIZE);
				os_snprintf(res_scan_buf, SCAN_BUFF_SIZE, "[  ");
				for(int i=0; i<g_scan_result.ap_num; i++)
				{
					if((i+1) == g_scan_result.ap_num)
					{
						os_snprintf(res_scan_buf+os_strlen(res_scan_buf), SCAN_BUFF_SIZE-os_strlen(res_scan_buf), "{\"name\": \"%s\"  }]", g_scan_result.aps[i].ssid);
					}
					else
					{
						os_snprintf(res_scan_buf+os_strlen(res_scan_buf), SCAN_BUFF_SIZE-os_strlen(res_scan_buf), "{\"name\": \"%s\"  },", g_scan_result.aps[i].ssid);
					}
				}
				break;
			}

			rtos_delay_milliseconds(200);
		}

		bk_printf("[%s][%d] scan %s\r\n", __FUNCTION__, __LINE__, res_scan_buf);
		webnet_session_write(session, (const uint8_t*)res_scan_buf, strlen(res_scan_buf));
	}

	if(res_scan_buf)
	{
		psram_free(res_scan_buf);
	}
    return;

}

int wifi_config_ok_notify(void)
{
    note_msg_t msg;

    bk_printf("[%s][%d] enter\n", __FUNCTION__, __LINE__);
    msg.event = ACK_WIFI_CONNECTED_CFG_FIN;
    wifi_send_notify(&msg);

    return BK_OK;
}

static void captive_dns_init(void) 
{
    int ret = 0;
    
    BK_LOGD(TAG, "captive dns init\n");

    ret = app_ipc_send_sync(IPC_CMD_CAPTIVE_DNS_START, NULL, 0, NULL);
    if (ret != BK_OK) {
        BK_LOGE(TAG, "%s IPC send failed, ret: %d\n", __func__, ret);
    }
}

static void captive_dns_deinit(void) 
{
    int ret = 0;
    
    BK_LOGD(TAG, "captive dns deinit\n");

    ret = app_ipc_send_sync(IPC_CMD_CAPTIVE_DNS_STOP, NULL, 0, NULL);
    if (ret != BK_OK) {
        BK_LOGE(TAG, "%s IPC send failed, ret: %d\n", __func__, ret);
    }
}

static void wifi_softap_cfg_thread(beken_thread_arg_t *arg)
{
	int ret= 0;
	note_msg_t msg;
	while(1)
	{
		rtos_delay_milliseconds(500);
		ret = rtos_pop_from_queue(&wifi_notify_que, (void *)&msg, BEKEN_WAIT_FOREVER);
		if(ret == BK_OK)
		{
			bk_printf("get msg :%d \n",msg.event);
			switch (msg.event)
			{
			case ACK_WIFI_CONNECT_START:
				wifi_sta_init();
				break;
			case ACK_WIFI_CONNECTED_CFG_FIN:
				bk_wifi_ap_stop();
				goto exit;
				break;
			default:
				break;
			}
		}
	}
exit:
	webnet_deinit();

	if(wifi_notify_que!= NULL)
	{
		rtos_deinit_queue(&wifi_notify_que);
		wifi_notify_que = NULL;
	}
	webnet_cgi_unregister();
	wifi_event_handler_deinit();
	wifi_cfg_thread = NULL;

	bk_event_unregister_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, _wifi_scan_event_cb);
	_scan_free_result();

	captive_dns_deinit();
	
	rtos_delete_thread(NULL);

}

void wificonfig_test(void)
{
	int ret = 0;
	//wifi_event_handler_init();
	wifi_softap_init();

	captive_dns_init();

	bk_event_register_cb(EVENT_MOD_WIFI, EVENT_WIFI_SCAN_DONE, _wifi_scan_event_cb, NULL);
#ifdef WEBNET_USING_CGI
	webnet_cgi_set_root("/");
	webnet_cgi_register("", cgi_web_ap_config_handler);
    webnet_cgi_register("scan", cgi_web_scan_handler);
#endif
	
	webnet_init();
	ret = rtos_init_queue(&wifi_notify_que,
							"dwifi_notify",
							sizeof(note_msg_t),
							5);

	if (ret != BK_OK)
	{
		bk_printf("%s, init queue failed\r\n", __func__);
		return ;
	}
	else
	{
		bk_printf("%s, init queue done\r\n", __func__);
	}
    ret = rtos_create_thread(&wifi_cfg_thread,
							5,
							"wificonfig",
							(beken_thread_function_t) wifi_softap_cfg_thread,
							4096, 
							(beken_thread_arg_t)NULL);

    if (ret != BK_OK)
    {
    	 bk_printf("%s, create thread failed\r\n", __func__);
    }                                 
    else
    {
    	 bk_printf("%s,create thread done\r\n", __func__);
    }
}