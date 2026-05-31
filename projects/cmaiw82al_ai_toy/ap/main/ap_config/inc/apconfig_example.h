#ifndef  _AP_CONFIG_EXAMPLE_H
#define  _AP_CONFIG_EXAMPLE_H

#define BK_DEFAULT_SSID 	"BEKEN_SSID"
#define BK_DEFAULT_PASS		"123456789"

#define PAGE_HEADER  "<html> \
\
<head> \
<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>\
<meta name='viewport' content='width=device-width, initial-scale=1.0'>\
<title>WIFI配置</title>\
   \
<style>\
		/* 基础样式：去除所有超链接的下划线 */\
        a {\
            text-decoration: none; /* 去除下划线 */\
            color: inherit; /* 继承父元素颜色（默认蓝色会消失） */\
        }\
\
        /* 悬停状态 */\
        a:hover {\
            text-decoration: none; /* 鼠标悬停时也不显示下划线 */\
            color: #333; /* 自定义悬停颜色（可选） */\
        }\
\
        /* 已访问状态 */\
        a:visited {\
            color: inherit; /* 保持与未访问时一致 */\
        }\
\
        /* 激活状态（点击瞬间） */\
        a:active {\
            color: inherit;\
        }\
        \
        /* 模态对话框的样式 */\
        .modal {\
            display: none; /* 默认隐藏 */\
            position: fixed;\
            z-index: 1;\
            left: 0;\
            top: 0;\
            width: 100%;\
            height: 100%;\
            overflow: auto;\
            background-color: rgb(0,0,0);\
            background-color: rgba(0,0,0,0.4); /* 黑色背景带有40%的不透明度 */\
        }\
\
        /* 模态对话框内容的样式 */\
        .modal-content {\
            background-color: #fefefe;\
            margin: 15% auto; /* 15% 距离顶部，水平居中 */\
            padding: 20px;\
            border: 1px solid #888;\
            width: 80%; /* 可根据需要调整宽度 */\
            max-width: 500px; /* 最大宽度 */\
        }\
\
        /* 关闭按钮的样式 */\
        .close {\
            color: #aaa;\
            float: right;\
            font-size: 28px;\
            font-weight: bold;\
        }\
\
        .close:hover,\
        .close:focus {\
            color: black;\
            text-decoration: none;\
            cursor: pointer;\
        }\
        \
        *,*::before,*::after {box-sizing: border-box;}html {line-height: 1.15;-webkit-text-size-adjust: 100%;}body {margin: 0;font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI',Arial;line-height: 1.5;background-color: #f3f3f3;padding-bottom: 200px;align-items: top;-ms-flex-align: top;display: flex;}p {margin-top: 0;margin-bottom: 1rem;}button{cursor: pointer;}.h3 {margin-top: 0;margin-bottom: 0.5rem;font-weight: 500;line-height: 1.2;font-size: 1.75rem;}.form-control {width: 100%;height: calc(1.5em + 0.75rem + 2px);padding: 0.375rem 0.75rem;font-size: 1rem;font-weight: 400;line-height: 1.5;color: #495057;background-color: #fff;background-clip: padding-box;border: 1px solid #ced4da;border-radius: 0.25rem;transition: border-color 0.15s ease-in-out, box-shadow 0.15s ease-in-out;}.form-control::-ms-expand {background-color: transparent;border: 0;}.form-control:focus {color: #495057;background-color: #fff;border-color: #80bdff;outline: 0;box-shadow: 0 0 0 0.2rem rgba(0, 123, 255, 0.25);}.form-control::-webkit-input-placeholder {color: #6c757d;opacity: 1;}.form-control::-moz-placeholder {color: #6c757d;opacity: 1;}.form-control:-ms-input-placeholder {color: #6c757d;opacity: 1;}.form-control::-ms-input-placeholder {color: #6c757d;opacity: 1;}.form-control:disabled {background-color: #e9ecef;opacity: 1;}.btn {color: #fff;background-color: #007bff;border-color: #007bff;padding: 0.5rem 1rem;font-size: 1.25rem;line-height: 1.5;border-radius: 0.3rem;display: block;width: 100%;}.mb-3 {margin-bottom: 1rem !important;}.mb-4 {margin-bottom: 1.5rem !important;}.text-center {text-align: center !important;}.font-weight-normal {font-weight: 400 !important;}html,body {height: 100%;}.form-signin {width: 100%;max-width: 420px;padding: 15px;margin: auto;}.form-label-group {position: relative;margin-bottom: 1rem;}.form-label-group > input,.form-label-group > label {height: 3.125rem;padding: .75rem;}.form-label-group > label {position: absolute;top: 0;left: 0;display: block;width: 100%;margin-bottom: 0;line-height: 1.5;color: #495057;pointer-events: none;cursor: text;border: 1px solid transparent;border-radius: .25rem;transition: all .1s ease-in-out;}.form-label-group input::-webkit-input-placeholder {color: transparent;}.form-label-group input:-ms-input-placeholder {color: transparent;}.form-label-group input::-ms-input-placeholder {color: transparent;}.form-label-group input::-moz-placeholder {color: transparent;}.form-label-group input::placeholder {color: transparent;}.form-label-group input:not(:placeholder-shown) {padding-top: 1.25rem;padding-bottom: .25rem;}.form-label-group input:not(:placeholder-shown) ~ label {padding-top: .25rem;padding-bottom: .25rem;font-size: 12px;color: #777;}@supports (-ms-ime-align: auto) {.form-label-group > label {display: none;}.form-label-group input::-ms-input-placeholder {color: #777;}}@media all and (-ms-high-contrast: none), (-ms-high-contrast: active) {.form-label-group > label {display: none;}.form-label-group input:-ms-input-placeholder {color: #777;}}\
    </style>\
</head>"

#define PAGE_BODY_GET "<body marginwidth='0' marginheight='0' onload='loadUserData()'>\
\
<div align='center' style='width: 100%; vertical-align: top; padding:0px'>\
    <div align='center' style='width: 90%; vertical-align: top; background-color: #ffffff; padding:0px; margin-top:30px;'>\
		<p id='step_tile' style='padding:0px; margin-top:10px; margin-left:10px; margin-right:10px; text-align:left; font-size: 1.75rem; '>正在获取wifi列表</p>\
		　\
		<div id='resultContainer' style='padding:0px; margin-left:10px; margin-right:10px; font-size: 1.2rem; text-align:left'></div>\
	</div>\
</div>\
\
<!-- 模态对话框的HTML结构 -->\
    <div id='myModal' class='modal'>\
        <div class='modal-content'>\
            <span class='close' id='close'>×</span>\
            <form method='post' action='/' class='form-signin'><div class='text-center mb-4'><h1 class='h3 mb-3 font-weight-normal'>\
				WIFI配置</h1></div><p>1. 请输入wifi密码，如果密码为空可以不填.</p><div class='form-label-group'><input type='text' name='ssid' id='ssid' class='form-control' placeholder='wifi名称' required autofocus readonly><label for='inputEmail'>wifi名称</label></div><div class='form-label-group'><input type='text' id='password' name='password' class='form-control' placeholder='wifi密码'><label for='inputPassword'>wifi密码</label></div><button class='btn' type='submit'>\
				确定</button></form>\
        </div>\
    </div>\
\
    <script>\
        const resultContainer = document.getElementById('resultContainer');\
\
        const API_URL = '/scan';\
        \
       function change_ssid(item) \
  		{\
  			document.getElementById('myModal').style.display = 'block';\
  			document.getElementById('ssid').value = item.dataset.ssid || '';\
  		}\
\
        function renderUsers(users) {\
            resultContainer.innerHTML = '';\
\
            users.forEach(user => {\
                const card = document.createElement('div');\
                card.className = 'user-card';\
\
                const link = document.createElement('a');\
                link.href = 'javascript:void(0)';\
                link.style.display = 'block';\
                link.style.width = '100%';\
                link.dataset.ssid = user.name;\
                link.textContent = user.name;\
                link.onclick = function() { change_ssid(this); };\
\
                const separator = document.createElement('hr');\
                separator.style.height = '1px';\
                separator.style.borderWidth = '0';\
                separator.style.color = '#F0F0F0';\
                separator.style.backgroundColor = '#F0F0F0';\
\
                card.appendChild(link);\
                card.appendChild(separator);\
                resultContainer.appendChild(card);\
            });\
        }\
\
\
        async function loadUserData() {\
            try {\
            	\
                document.getElementById('step_tile').innerHTML = '正在获取wifi列表';\
\
                const response = await fetch(API_URL);\
\
                if (!response.ok) {\
                    throw new Error(`HTTP 错误！状态码：${response.status}`);\
                }\
\
                const users = await response.json();\
\
				document.getElementById('step_tile').innerHTML = '请选择您的WIFI';\
                renderUsers(users);\
\
            } catch (error) {\
                console.error('获取数据失败:', error);\
                resultContainer.innerHTML = `\
                    <div class='error'>\
                        加载失败: ${error.message}\
                    </div>\
                `;\
            }\
        }\
\
        \
        document.getElementById('close').addEventListener('click', function() {\
            document.getElementById('myModal').style.display = 'none';\
        });\
\
        window.onclick = function(event) {\
            if (event.target == document.getElementById('myModal')) {\
            	document.getElementById('myModal').style.display = 'none';\
            }\
        };\
    </script>\
</body></html>\r\n"

#define MAX_WIFI_SCAN_AP_NUM             32
#define SCAN_BUFF_SIZE                   4096
#define SCAN_OVER_TIME                   3000

enum {
	ACK_WIFI_CONNECT_START ,
	ACK_WIFI_CONNECTED_CFG_FIN,
};

typedef struct
{
    char ssid[33];
    char pwd[65];
}net_info_t;

typedef struct
{
	uint32_t event;
	uint32_t param;
} note_msg_t;

int wifi_send_notify(note_msg_t *arg);
void wificonfig_test(void);

#endif
