#ifndef __MCP_PROPERTY_H__
#define __MCP_PROPERTY_H__
#include <os/os.h>
#include "common.h"
#include "dl_list.h"
#include "cJSON.h"

typedef enum {
    PROPERTY_TYPE_BOOL,
    PROPERTY_TYPE_INT,
    PROPERTY_TYPE_STR,
}PROPERTY_TYPE;
    
typedef struct{
    int id;
    char* func_name;
    char* arg_json;
}mcp_cf_t;

typedef struct{
    char* name;
    PROPERTY_TYPE type;
    union {
        bool bool_value;
        int int_value;
        char* string_value;
    } value;
    bool has_default_val;
    bool has_max_val;
    bool has_min_val;
    int max_val;
    int min_val;
    
    struct dl_list      m_list;
}property_t;


typedef struct{
    char* name;
    char* description;

    struct dl_list property_head;
    beken_mutex_t property_mutex;
    void (*func_cb)(mcp_cf_t*);
    
    struct dl_list      m_list;
}property_list_t;

char* mcp_property_to_json(property_t* property);
char* mcp_property_list_to_json(property_list_t* list);
void mcp_property_del_by_name(property_list_t* list, char* name);
property_t* mcp_property_get_by_name(property_list_t* list, char* name);
int mcp_property_list_add_node(property_list_t* list, property_t* property);
property_list_t* mcp_property_list_init(void);
void mcp_property_list_deinit(property_list_t* list);

#endif
