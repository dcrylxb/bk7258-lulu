/*
 * File      : webnet.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively for commercial application, you can contact us
 * by email <business@rt-thread.com> for commercial license.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2011-08-02     Bernard      the first version
 * 2012-07-03     Bernard      Add webnet port and webroot setting interface.
 */

#include <stdint.h>
#include <string.h>

#include <webnet.h>
#include <wn_module.h>
#include <components/log.h>

#include <os/os.h>

#include <sys/select.h>

#if 0
#if defined(RT_USING_LWIP) && (RT_LWIP_TCPTHREAD_STACKSIZE < 1408)
#error The lwIP tcpip thread stack size(RT_LWIP_TCPTHREAD_STACKSIZE) must more than 1408
#endif
#endif

#define TAG  "webnet"

static beken_thread_t wb_thr = NULL;
static uint16_t webnet_port = WEBNET_PORT;
static char webnet_root[64] = WEBNET_ROOT;
static bool init_ok = 0;

void webnet_set_port(int port)
{
    BK_ASSERT(init_ok == 0);
    webnet_port = port;
}

int webnet_get_port(void)
{
    return webnet_port;
}

void webnet_set_root(const char* webroot_path)
{
    strncpy(webnet_root, webroot_path, sizeof(webnet_root) - 1);
    webnet_root[sizeof(webnet_root) - 1] = '\0';
}

const char* webnet_get_root(void)
{
    return webnet_root;
}

/**
 * webnet thread entry
 */
int flag_close = 0;
static void webnet_thread(void *parameter)
{
    int listenfd = -1;
    fd_set readset, tempfds;
    fd_set writeset, tempwrtfds;
    int sock_fd, maxfdp1;
    struct sockaddr_in webnet_saddr;

    /* First acquire our socket for listening for connections */
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
    {
        BK_LOGE(TAG,"Create socket failed.");
        goto __exit;
    }

    os_memset(&webnet_saddr, 0, sizeof(webnet_saddr));
    webnet_saddr.sin_family = AF_INET;
    webnet_saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    webnet_saddr.sin_port = htons(webnet_port); /* webnet server port */

    if (bind(listenfd, (struct sockaddr *) &webnet_saddr, sizeof(webnet_saddr)) == -1)
    {
        BK_LOGI(TAG,"Bind socket failed, errno=%d\n", errno);
        goto __exit;
    }

    /* Put socket into listening mode */
    if (listen(listenfd, WEBNET_CONN_MAX) == -1)
    {
        BK_LOGI(TAG,"Socket listen(%d) failed.", WEBNET_CONN_MAX);
        goto __exit;
    }

    /* initialize module (no session at present) */
    webnet_module_handle_event(NULL, WEBNET_EVENT_INIT);
    /* Wait forever for network input: This could be connections or data */
    for (;;)
    {
        /* Determine what sockets need to be in readset */
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_SET(listenfd, &readset);
        /* set fds in each sessions */
        maxfdp1 = webnet_sessions_set_fds(&readset, &writeset);
        if (maxfdp1 < listenfd + 1)
        {
            maxfdp1 = listenfd + 1;
        }
		if(flag_close == 1)
		{
			goto __exit;
		}
        /* use temporary fd set in select */
        tempfds = readset;
        tempwrtfds = writeset;
        /* Wait for data or a new connection */
		
        sock_fd = select(maxfdp1, &tempfds, &tempwrtfds, 0, 0);
        if (sock_fd == 0)
        {
            continue;
        }
        /* At least one descriptor is ready */
        if (FD_ISSET(listenfd, &tempfds))
        {
            struct webnet_session* accept_session;
            /* We have a new connection request */
            accept_session = webnet_session_create(listenfd);
            if (accept_session == NULL)
            {
                /* create session failed, just accept and then close */
                int sock;
                struct sockaddr cliaddr;
                socklen_t clilen;

                clilen = sizeof(struct sockaddr_in);
                sock = accept(listenfd, &cliaddr, &clilen);
                if (sock >= 0)
                {
                    closesocket(sock);
                }
            }
            else
            {
                /* add read fdset */
                FD_SET(accept_session->socket, &readset);
            }
        }
        webnet_sessions_handle_fds(&tempfds, &writeset);
    }

__exit:
    if (listenfd >= 0)
    {
        closesocket(listenfd);
    }
	flag_close = 0;
	if(wb_thr!= NULL)
	{
		wb_thr = NULL;
		rtos_delete_thread(NULL);
	}
}

int webnet_init(void)
{
    int ret=0;

    if (init_ok == 1)
    {
        BK_LOGI(TAG,"Thread webnet package is already initialized.");
        return 0;
    }

	ret = rtos_create_thread(&wb_thr,
								5,
								"webnet",
								(beken_thread_function_t)webnet_thread,
								4096, 
								(beken_thread_arg_t) NULL) ;

    if (ret == BK_OK)
    {
        init_ok = 1;
        BK_LOGI(TAG,"Thread webnet package (V%s) initialize success.", WEBNET_VERSION);
    }
    else
    {
        BK_LOGE(TAG,"Thread webnet package (V%s) initialize failed.", WEBNET_VERSION);
		wb_thr = NULL;
        return -1;
    }
	
    return 0;
}
int webnet_deinit(void)
{
	if (init_ok == 1)
		init_ok = 0;
	flag_close = 1;
	return 0;
}

