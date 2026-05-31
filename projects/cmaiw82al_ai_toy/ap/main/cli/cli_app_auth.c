#include "cli.h"
#include "bk_cli.h"
#include "device_auth.h"
#include <os/str.h>

static void cli_auth_help(void)
{
    CLI_RAW_LOGI("\r\nauth status|secret|sign\n");
    CLI_RAW_LOGI("  Device authentication commands.\n");
    CLI_RAW_LOGI("  Available commands:\n");
    CLI_RAW_LOGI("    status - Show device secret provisioning status\n");
    CLI_RAW_LOGI("    secret - Provision device secret\n");
    CLI_RAW_LOGI("    sign   - Sign a binding message for diagnostics\n");
    CLI_RAW_LOGI("    help   - Show this help message\n");
    CLI_RAW_LOGI("  Examples:\n");
    CLI_RAW_LOGI("    auth status\n");
    CLI_RAW_LOGI("    auth secret <device_secret>\n");
    CLI_RAW_LOGI("    auth sign <signing_message>\n");
}

static void cli_auth_status(void)
{
    char secret[DEVICE_AUTH_SECRET_MAX_LEN];

    if (device_auth_get_secret(secret, sizeof(secret)) == BK_OK) {
        CLI_LOGI("device secret status: provisioned len:%u\r\n", os_strlen(secret));
    } else {
        CLI_LOGW("device secret status: not provisioned\r\n");
    }
}

static void cli_auth_secret(int argc, char **argv)
{
    if (argc != 3) {
        CLI_LOGW("usage: auth secret <device_secret>\r\n");
        return;
    }

    if (device_auth_set_secret(argv[2]) == BK_OK) {
        CLI_LOGI("device secret provisioned len:%u\r\n", os_strlen(argv[2]));
    } else {
        CLI_LOGE("device secret provision failed\r\n");
    }
}

static void cli_auth_sign(int argc, char **argv)
{
    char signature[DEVICE_AUTH_SIGNATURE_HEX_LEN + 1];

    if (argc != 3) {
        CLI_LOGW("usage: auth sign <signing_message>\r\n");
        return;
    }

    if (device_auth_sign_message(argv[2], signature, sizeof(signature)) == BK_OK) {
        CLI_LOGI("signature: %s\r\n", signature);
    } else {
        CLI_LOGE("signature failed; check device secret\r\n");
    }
}

static void cli_auth_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if ((argc < 2) || (os_strcmp(argv[1], "help") == 0)) {
        cli_auth_help();
        return;
    }

    if (os_strcmp(argv[1], "status") == 0) {
        cli_auth_status();
    } else if (os_strcmp(argv[1], "secret") == 0) {
        cli_auth_secret(argc, argv);
    } else if (os_strcmp(argv[1], "sign") == 0) {
        cli_auth_sign(argc, argv);
    } else {
        CLI_LOGW("invalid auth command: %s\r\n", argv[1]);
        cli_auth_help();
    }
}

#define AUTH_CMD_CNT (sizeof(s_auth_commands) / sizeof(struct cli_command))
static const struct cli_command s_auth_commands[] = {
    {"auth", "auth status|secret|sign", cli_auth_cmd},
};

int cli_app_auth_init(void)
{
    return cli_register_commands(s_auth_commands, AUTH_CMD_CNT);
}
