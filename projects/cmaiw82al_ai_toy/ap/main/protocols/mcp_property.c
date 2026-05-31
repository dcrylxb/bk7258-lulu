#include "mcp_property.h"
#include <os/str.h>
#include <os/mem.h>

#define TAG "mcp"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

char* mcp_property_to_json(property_t* property)
{
    cJSON* json = cJSON_CreateObject();

    if(property == NULL)
    {
        LOGE("null property\r\n");
        cJSON_Delete(json);
        return NULL;
    }

    if(PROPERTY_TYPE_BOOL == property->type)
    {
        cJSON_AddStringToObject(json, "type", "boolean");
        if(property->has_default_val)
        {
            cJSON_AddBoolToObject(json, "default", property->value.bool_value);
        }
    }

    if(PROPERTY_TYPE_INT == property->type)
    {
        cJSON_AddStringToObject(json, "type", "integer");
        if(property->has_default_val)
        {
            cJSON_AddNumberToObject(json, "default", property->value.int_value);
        }
        if(property->has_max_val)
        {
            cJSON_AddNumberToObject(json, "maximum", property->max_val);
        }
        if(property->has_min_val)
        {
            cJSON_AddNumberToObject(json, "minimum", property->min_val);
        }
    }

    if(PROPERTY_TYPE_STR == property->type)
    {
        cJSON_AddStringToObject(json, "type", "string");
        if(property->has_default_val)
        {
            cJSON_AddStringToObject(json, "default", property->value.string_value);
        }
    }

    char* js_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    LOGD("json: %s\r\n", js_str);
    return js_str;
}

char* mcp_property_list_to_json(property_list_t* list)
{
    property_t *tmp, *n;
    cJSON *json = cJSON_CreateObject();
    cJSON* prop_json;
    char* tmp_str = NULL;
    
    if(list == NULL)
        {
            LOGE("null property\r\n");
            cJSON_Delete(json);
            return NULL;
        }

    rtos_lock_mutex(&list->property_mutex);
    dl_list_for_each_safe(tmp, n, &(list->property_head), property_t, m_list)
    {
        LOGD("%s %p %p\r\n", tmp->name, &(list->property_head), tmp->m_list);
        if(tmp != NULL)
        {
            tmp_str = mcp_property_to_json(tmp);
            if(tmp_str != NULL)
            {
                prop_json = cJSON_Parse(tmp_str);
                cJSON_AddItemToObject(json, tmp->name, prop_json);
                cJSON_free(tmp_str);
            }
        }
    }
    rtos_unlock_mutex(&list->property_mutex);

    char* json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    LOGD("json: %s\r\n", json_str);
    return json_str;
}

void mcp_property_del_by_name(property_list_t* list, char* name)
{
    property_t *tmp, *n;
    
    rtos_lock_mutex(&list->property_mutex);
    dl_list_for_each_safe(tmp, n, &(list->property_head), property_t, m_list)
    {
        if(!os_strcmp(name, tmp->name))
        {
            bk_printf("[%s][%d]this property: %s is free\r\n", __FUNCTION__, __LINE__, tmp->name);
            dl_list_del(&tmp->m_list);
            os_free(tmp);
        }
    }
    rtos_unlock_mutex(&list->property_mutex);

    LOGE("no property named: %s\r\n", name);
    return;
}

property_t* mcp_property_get_by_name(property_list_t* list, char* name)
{
    property_t *tmp, *n;
    
    rtos_lock_mutex(&list->property_mutex);
    dl_list_for_each_safe(tmp, n, &list->property_head, property_t, m_list)
    {
        if(!os_strcmp(name, tmp->name))
        {
            return tmp;
        }
    }
    rtos_unlock_mutex(&list->property_mutex);

    return NULL;
}

int mcp_property_list_add_node(property_list_t* list, property_t* property)
{
    int ret = 0;
    property_t* node;

    do
    {
        node = (property_t*)os_malloc(sizeof(property_t));
        if(node == NULL)
        {
            LOGE("malloc failed\r\n");
            ret = -1;
            break;
        }

        os_memcpy(node, property, sizeof(property_t));
        rtos_lock_mutex(&list->property_mutex);
        dl_list_add_tail(&list->property_head, &node->m_list);
        rtos_unlock_mutex(&list->property_mutex);
    }while(0);

    return ret;
}

property_list_t* mcp_property_list_init(void)
{
    int ret = 0;
    property_list_t* list = NULL;
    do{
        list = (property_list_t*)os_malloc(sizeof(property_list_t));
        if(list == NULL)
        {
            LOGE("malloc failed\r\n");
            break;
        }
        ret = rtos_init_mutex(&(list->property_mutex));
        if(ret != 0)
        {
            LOGE("mutex init failed\r\n");
            os_free(list);
            list = NULL;
            break;
        }
        dl_list_init(&list->property_head);
    }while(0);

    return list;
}

void mcp_property_list_deinit(property_list_t* list)
{
    property_t *tmp, *n;

    rtos_lock_mutex(&list->property_mutex);
    dl_list_for_each_safe(tmp, n, &(list->property_head), property_t, m_list)
    {
        if(tmp != NULL)
        {
            os_free(tmp);
        }
    }
    rtos_unlock_mutex(&list->property_mutex);
    
    rtos_deinit_mutex(&(list->property_mutex));
    os_free(list);
}
