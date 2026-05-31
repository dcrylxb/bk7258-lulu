#include "components/log.h"
#include "os/str.h"
#include "cli.h"

#include "bk_app_asr.h"

static void cli_asr_auth_help(void)
{
    CLI_RAW_LOGI("asr_auth <auth_id> [auth_key]\r\n");
}

static void cli_asr_auth_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    int ret = BK_OK;
    asr_auth_t auth = {0};

    if (argc == 1) {
        ret = bk_app_asr_get_auth(&auth);
        if (ret != BK_OK) {
            CLI_LOGE("bk_app_asr_get_auth fail, ret: %d\r\n", ret);
        } else {
            CLI_LOGI("auth_id: %s, auth_key: %s\r\n", auth.id, auth.key);
        }
        return;
    } else if (argc == 2) {
        CLI_LOGI("auth_id: %s\r\n", argv[1]);
        os_strncpy(auth.id, argv[1], sizeof(auth.id));
    } else if (argc == 3) {
        CLI_LOGI("auth_id: %s\r\n", argv[1]);
        CLI_LOGI("auth_key: %s\r\n", argv[2]);
        os_strncpy(auth.id, argv[1], sizeof(auth.id));
        os_strncpy(auth.key, argv[2], sizeof(auth.key));
    } else {
        cli_asr_auth_help();
        return;
    }

    ret = bk_app_asr_set_auth(&auth);
    if (ret != BK_OK) {
        CLI_LOGE("bk_app_asr_set_auth fail, ret: %d\r\n", ret);
    } else {
        CLI_LOGI("bk_app_asr_set_auth success\r\n");
    }

    return;
}

static const struct cli_command asr_auth_cmd[] = {
    {"asr_auth", "asr authentication commands", cli_asr_auth_cmd},
};

int cli_app_asr_auth_init(void)
{
    return cli_register_commands(asr_auth_cmd, sizeof(asr_auth_cmd) / sizeof(struct cli_command));
}