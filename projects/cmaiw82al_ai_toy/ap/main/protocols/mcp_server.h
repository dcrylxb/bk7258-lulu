#ifndef __MCP_SERVER_H__
#define __MCP_SERVER_H__
#include "mcp_property.h"

#define MCP_PAYLOAD_MAX     8000

typedef struct{
    struct dl_list tools_head;
    bool mcp_is_start;

    super_module_t super;
    int (*add_tool)(property_list_t*);
    void (*get_tool_list)(int, char*);
    void (*reply_result)(int, char*);
    void (*reply_error)(int, char*);
    void (*reply_tool_list)(int, char*);
    void (*recv_msg_cb)(cJSON*);
    char* (*get_explain_url)(void);
    char* (*get_explain_token)(void);
}mcp_server_t;

mcp_server_t *mcp_server_instance(void);

#endif
