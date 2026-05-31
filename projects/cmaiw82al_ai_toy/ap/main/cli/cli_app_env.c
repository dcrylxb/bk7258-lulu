#include <stdlib.h>

#include "cli.h"
#include "bk_cli.h"
#include "env_module.h"
#include "apconfig_example.h"

static void cli_env_help(void)
{
    CLI_RAW_LOGI("\r\nenv [command]\n");
    CLI_RAW_LOGI("  Environment commands.\n");
    CLI_RAW_LOGI("  Available commands:\n");
    CLI_RAW_LOGI("    get_net - Get network information\n");
    CLI_RAW_LOGI("    set_net - Set network information\n");
    CLI_RAW_LOGI("    del_net - Delete network information\n");
    CLI_RAW_LOGI("    get_spk - Get speaker information\n");
    CLI_RAW_LOGI("    set_spk - Set speaker information\n");
    CLI_RAW_LOGI("    del_spk - Delete speaker information\n");
    CLI_RAW_LOGI("    help    - Show this help message\n");
    CLI_RAW_LOGI("  Examples:\n");
    CLI_RAW_LOGI("    env get_net\n");
    CLI_RAW_LOGI("    env set_net <ssid> <password>\n");
    CLI_RAW_LOGI("    env del_net\n");
    CLI_RAW_LOGI("    env get_spk\n");
    CLI_RAW_LOGI("    env set_spk <value>\n");
    CLI_RAW_LOGI("    env del_spk\n");
    CLI_RAW_LOGI("    env help\n");
}

static void cli_env_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if ((argc == 2) && (!os_strncmp(argv[1], "help", 4))) {
        cli_env_help();
        return;
    }

    if (argc < 2) {
        CLI_LOGW("invalid argc number\n");
        cli_env_help();
        return;
    }

    if (os_strcmp(argv[1], "get_net") == 0) {
        if (env_module_instance() && env_module_instance()->getNetInfo) {
            net_info_t net_info = {0};
            int ret = env_module_instance()->getNetInfo(&net_info);
            if (ret == 0) {
                CLI_LOGI("Network information:\n");
                CLI_LOGI("  SSID: %s\n", net_info.ssid);
                CLI_LOGI("  Password: %s\n", net_info.pwd);
            } else {
                CLI_LOGE("Failed to get network information\n");
            }
        } else {
            CLI_LOGE("Failed to get network information\n");
        }
    } else if (os_strcmp(argv[1], "set_net") == 0) {
        if (argc != 4) {
            CLI_LOGW("invalid parameters for set_net\n");
            CLI_LOGW("Usage: env set_net <ssid> <password>\n");
            return;
        }
        
        if (env_module_instance() && env_module_instance()->setNetInfo) {
            net_info_t net_info = {0};
            os_strncpy(net_info.ssid, argv[2], sizeof(net_info.ssid) - 1);
            os_strncpy(net_info.pwd, argv[3], sizeof(net_info.pwd) - 1);
            
            int ret = env_module_instance()->setNetInfo(&net_info);
            if (ret == 0) {
                CLI_LOGI("Network information set successfully\n");
            } else {
                CLI_LOGE("Failed to set network information\n");
            }
        } else {
            CLI_LOGE("Failed to set network information\n");
        }
    } else if (os_strcmp(argv[1], "del_net") == 0) {
        if (env_module_instance() && env_module_instance()->delNetInfo) {
            CLI_LOGI("Deleting network information...\n");
            env_module_instance()->delNetInfo();
            CLI_LOGI("Network information deleted successfully\n");
        } else {
            CLI_LOGE("Failed to delete network information\n");
        }
    } else if (os_strcmp(argv[1], "get_spk") == 0) {
        if (env_module_instance() && env_module_instance()->getSpkInfo) {
            int spk_info = 0;
            int ret = env_module_instance()->getSpkInfo(&spk_info);
            if (ret == 0) {
                CLI_LOGI("Speaker information: %d\n", spk_info);
            } else {
                CLI_LOGE("Failed to get speaker information\n");
            }
        } else {
            CLI_LOGE("Failed to get speaker information\n");
        }
    } else if (os_strcmp(argv[1], "set_spk") == 0) {
        if (argc != 3) {
            CLI_LOGW("invalid parameters for set_spk\n");
            CLI_LOGW("Usage: env set_spk <value>\n");
            return;
        }
        
        if (env_module_instance() && env_module_instance()->setSpkInfo) {
            int spk_info = atoi(argv[2]);
            int ret = env_module_instance()->setSpkInfo(&spk_info);
            if (ret == 0) {
                CLI_LOGI("Speaker information set successfully\n");
            } else {
                CLI_LOGE("Failed to set speaker information\n");
            }
        } else {
            CLI_LOGE("Failed to set speaker information\n");
        }
    } else if (os_strcmp(argv[1], "del_spk") == 0) {
        if (env_module_instance() && env_module_instance()->delSpkInfo) {
            CLI_LOGI("Deleting speaker information...\n");
            env_module_instance()->delSpkInfo();
            CLI_LOGI("Speaker information deleted successfully\n");
        } else {
            CLI_LOGE("Failed to delete speaker information\n");
        }
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_env_help();
    } else {
        CLI_LOGW("invalid parameter: %s\n", argv[1]);
        cli_env_help();
        return;
    }
}

#define ENV_CMD_CNT (sizeof(s_env_commands) / sizeof(struct cli_command))
static const struct cli_command s_env_commands[] = {
    {"env", "env [command]", cli_env_cmd},
};

int cli_app_env_init(void)
{
    return cli_register_commands(s_env_commands, ENV_CMD_CNT);
}