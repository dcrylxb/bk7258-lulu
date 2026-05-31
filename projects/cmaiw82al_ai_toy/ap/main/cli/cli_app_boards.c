#include <stdio.h>
#include <string.h>
#include "cli.h"
#include "bk_cli.h"
#include "boards_common.h"
#include <modules/fdk_aac_enc/aacenc_lib.h>

static void cli_hw_help(void)
{
    CLI_RAW_LOGI("\r\nhw [mac|uuid|sub_mac|deep_sleep]\n");
    CLI_RAW_LOGI("  Get hardware information or control hardware.\n");
    CLI_RAW_LOGI("  -mac: Get device MAC address.\n");
    CLI_RAW_LOGI("  -uuid: Get device UUID.\n");
    CLI_RAW_LOGI("  -sub_mac: Get device sub MAC address.\n");
    CLI_RAW_LOGI("  -deep_sleep: Enter deep sleep mode.\n");
}

static void cli_hw_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    board_module_t *board = board_instance();
    
    if ((argc == 2) && (!os_strncmp(argv[1], "help", 4))) {
        cli_hw_help();
        return;
    }

    if (argc != 2) {
        CLI_LOGW("invalid argc number\n");
        cli_hw_help();
        return;
    }

    if (os_strcmp(argv[1], "mac") == 0) {
        if (board && board->getMac) {
            char *mac = board->getMac();
            CLI_LOGI("MAC: %s\n", mac);
        } else {
            CLI_LOGE("Failed to get MAC address\n");
            return;
        }
    } else if (os_strcmp(argv[1], "uuid") == 0) {
        if (board && board->getUid) {
            char *uuid = board->getUid();
            CLI_LOGI("UUID: %s\n", uuid);
        } else {
            CLI_LOGE("Failed to get UUID\n");
            return;
        }
    } else if (os_strcmp(argv[1], "sub_mac") == 0) {
        if (board && board->getSubMac) {
            char *sub_mac = board->getSubMac();
            CLI_LOGI("Sub MAC: %s\n", sub_mac);
        } else {
            CLI_LOGE("Failed to get sub MAC address\n");
            return;
        }
    } else if (os_strcmp(argv[1], "deep_sleep") == 0) {
        if (board && board->startSleep) {
            CLI_LOGI("Entering deep sleep mode...\n");
            board->startSleep();
        } else {
            CLI_LOGE("Failed to enter deep sleep mode\n");
            return;
        }
    } else if (os_strcmp(argv[1], "aac") == 0) {
        char *p = os_malloc(130*1024);
        if (p == NULL) {
            CLI_LOGE("malloc fail\n");
            return;
        }
        CLI_LOGI("befor aac, sram free size: %d\n", rtos_get_free_heap_size());
        CLI_LOGI("befor aac, psram free size: %d\n", rtos_get_psram_free_heap_size());
        HANDLE_AACENCODER handle;
        AACENC_ERROR ret = aacEncOpen(&handle, 1, 1);
        if (ret != AACENC_OK) {
            CLI_LOGE("open encoder fail, ret: %d\n", ret);
            os_free(p);
            return;
        }
        CLI_LOGI("after aac, sram free size: %d\n", rtos_get_free_heap_size());
        CLI_LOGI("befor aac, psram free size: %d\n", rtos_get_psram_free_heap_size());
        aacEncClose(&handle);
        os_free(p);
    } else {
        CLI_LOGW("invalid parameter: %s\n", argv[1]);
        cli_hw_help();
        return;
    }
}

#define HW_CMD_CNT (sizeof(s_hw_commands) / sizeof(struct cli_command))
static const struct cli_command s_hw_commands[] = {
    {"hw", "hw [mac|uuid|sub_mac|deep_sleep]", cli_hw_cmd},
};

int cli_app_hw_init(void)
{
    return cli_register_commands(s_hw_commands, HW_CMD_CNT);
}
