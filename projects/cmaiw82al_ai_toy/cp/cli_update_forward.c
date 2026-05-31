#include "cli.h"
#include <components/shell_task.h>
#include <os/str.h>

#define CMAIW82AL_CP_UPDATE_CMD_MAX 512

static void cli_update_forward_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    char cmd[CMAIW82AL_CP_UPDATE_CMD_MAX] = {0};
    int cmd_len = 0;

    (void)pcWriteBuffer;
    (void)xWriteBufferLen;

    if (argc <= 0 || argv == NULL) {
        CLI_LOGI("usage: update flash|eyes|ota|if0 <http-url>\r\n");
        CLI_LOGI("usage: eye list|play <avi>|carousel [seconds]|stop|verify [hash]\r\n");
        CLI_LOGI("usage: pet status|emote|event|action|haptic|motion|voice|privacy|idle\r\n");
        CLI_LOGI("usage: auth status|secret|sign\r\n");
        return;
    }

    for (int i = 0; i < argc; i++) {
        int remaining = CMAIW82AL_CP_UPDATE_CMD_MAX - cmd_len;
        int written = 0;

        if (argv[i] == NULL) {
            continue;
        }

        written = os_snprintf(cmd + cmd_len, remaining, "%s%s", (i == 0) ? "" : " ", argv[i]);
        if (written < 0 || written >= remaining) {
            CLI_LOGE("CP_UPDATE command too long\r\n");
            return;
        }
        cmd_len += written;
    }

    if (cmd_len + 2 >= CMAIW82AL_CP_UPDATE_CMD_MAX) {
        CLI_LOGE("CP_UPDATE command too long\r\n");
        return;
    }

    cmd[cmd_len++] = '\r';
    cmd[cmd_len++] = '\n';

    CLI_LOGI("CP_UPDATE forward to AP: %s", cmd);
    shell_cmd_forward(cmd, (u16)cmd_len);
}

#define UPDATE_FORWARD_CMD_CNT (sizeof(s_update_forward_commands) / sizeof(struct cli_command))

static const struct cli_command s_update_forward_commands[] = {
    {"update", "update flash|eyes|ota|if0 <http-url>", cli_update_forward_cmd},
    {"eye", "eye list|play <avi>|carousel [seconds]|stop|verify [hash]", cli_update_forward_cmd},
    {"pet", "pet status|emote|event|action|haptic|motion|voice|privacy|idle", cli_update_forward_cmd},
    {"auth", "auth status|secret|sign", cli_update_forward_cmd},
};

int cli_update_forward_init(void)
{
    return cli_register_commands(s_update_forward_commands, UPDATE_FORWARD_CMD_CNT);
}
