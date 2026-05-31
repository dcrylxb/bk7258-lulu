/*
 * File      : wn_module_cgi.c
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
 */

#include <string.h>

#include <webnet.h>
#include <wn_module.h>
#include <wn_utils.h>

#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif

#ifdef WEBNET_USING_CGI

#define CGI_ROOT_PATH_MAX	64

static char _cgi_root[CGI_ROOT_PATH_MAX] = {0};
struct webnet_cgi_item
{
    const char* name;
    void (*handler)(struct webnet_session* session);
};
static struct webnet_cgi_item* _cgi_items = NULL;
static uint32_t _cgi_count = 0;

void webnet_cgi_set_root(const char* root)
{
    if (os_strlen(root) > CGI_ROOT_PATH_MAX)
    {
        BK_ASSERT(0);
        return;
    }

    os_strncpy(_cgi_root, root, strlen(root));
    if (_cgi_root[strlen(_cgi_root)] != '/')
    {
        _cgi_root[strlen(_cgi_root) + 1] = '/';
        _cgi_root[strlen(_cgi_root) + 1] = '\0';
    }
}


void webnet_cgi_register(const char* name, void (*handler)(struct webnet_session* session))
{
    if (_cgi_items == NULL)
    {
        _cgi_count = 1;
        _cgi_items = (struct webnet_cgi_item*) wn_malloc (sizeof(struct webnet_cgi_item) * _cgi_count);
    }
    else
    {
        _cgi_count += 1;
        _cgi_items = (struct webnet_cgi_item*) wn_realloc (_cgi_items, sizeof(struct webnet_cgi_item) * _cgi_count);
    }

    BK_ASSERT(_cgi_items != NULL);
    _cgi_items[_cgi_count - 1].name = name;
    _cgi_items[_cgi_count - 1].handler = handler;
}


void webnet_cgi_unregister(void)
{
    if(_cgi_items)
    {
        wn_free(_cgi_items);
        _cgi_items = NULL;
    }

}

static void captive_dns_handler(struct webnet_session* session)
{
    for (int index = 0; index < _cgi_count; index ++)
    {
        if (0 == strlen(_cgi_items[index].name)) 
        {
            _cgi_items[0].handler(session);
            return;
        }
    }
}


int webnet_module_cgi(struct webnet_session* session, int event)
{
    if (event == WEBNET_EVENT_INIT)
    {
        /* set default cgi path */
        if (_cgi_root[0] == '\0')
        {
            strcpy(_cgi_root, "/cgi-bin/");
        }
    }
    else if (event == WEBNET_EVENT_URI_PHYSICAL)
    {
        struct webnet_request* request;
        char *cgi_path = NULL;

        BK_ASSERT(session != NULL);
        request = session->request;
        BK_ASSERT(request != NULL);
        /* check whether a cgi request */
        cgi_path = strstr(request->path, _cgi_root);
        if (cgi_path != NULL)
        {
            char* cgi_name;
            uint32_t index;

            cgi_name = cgi_path + strlen(_cgi_root);
            for (index = 0; index < _cgi_count; index ++)
            {
                if ((strlen(cgi_name) == strlen(_cgi_items[index].name))
                        && strncasecmp(cgi_name, _cgi_items[index].name, strlen(_cgi_items[index].name)) == 0)
                {
                    /* found it */
                    _cgi_items[index].handler(session);
                    return WEBNET_MODULE_FINISHED;
                }
            }

            //没找到网页时将所有页面跳转为/
            captive_dns_handler(session);
            return WEBNET_MODULE_FINISHED;

            /* set 404 not found error */
            request->result_code = 404;
        }
    }

    return WEBNET_MODULE_CONTINUE;
}

#endif /* WEBNET_USING_CGI */
