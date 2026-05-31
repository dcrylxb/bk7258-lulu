#ifndef __BOARDS_COMMON_H__
#define __BOARDS_COMMON_H__
#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include "common.h"

char* board_get_json_str(void);
char* board_get_mac(void);
char* board_get_uuid(void);

typedef struct{
    super_module_t super;
    char* (*getMac)(void);
    char* (*getUid)(void);
    char* (*getSubMac)(void);
    void (*startSleep)(void);
}board_module_t;

board_module_t *board_instance(void);

#endif
