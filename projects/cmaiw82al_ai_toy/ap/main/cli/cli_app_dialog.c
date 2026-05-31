#include <string.h>
#include "components/log.h"
#include "cli.h"
#include "dialog_module.h"
#include "bk_app_audio.h"

static void cli_dialog_help(void)
{
    CLI_RAW_LOGI("dialog [command] [options]\r\n");
    CLI_RAW_LOGI("  dump [start|stop] - Dump microphone data to file\r\n");
    CLI_RAW_LOGI("    start: Start dumping microphone data to file\r\n");
    CLI_RAW_LOGI("    stop : Stop dumping microphone data\r\n");
    CLI_RAW_LOGI("  play [file] - Play prompt tone file\r\n");
}

static void cli_dialog_dump_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 3) {
        cli_dialog_help();
        return;
    }

    if (os_strcmp(argv[2], "start") == 0) {
        if (dialog_module_instance()->dump_data_start) {
            dialog_module_instance()->dump_data_start();
        } else {
            CLI_LOGE("CONFIG_DUMP_AUDIO_TO_VFS is not enabled\r\n");
        }
    } else if (os_strcmp(argv[2], "stop") == 0) {
        if (dialog_module_instance()->dump_data_stop) {
            dialog_module_instance()->dump_data_stop();
        } else {
            CLI_LOGE("CONFIG_DUMP_AUDIO_TO_VFS is not enabled\r\n");
        }
    } else {
        cli_dialog_help();
        return;
    }
}

static void cli_dialog_play_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc != 3) {
        cli_dialog_help();
        return;
    }

    bk_app_audio_play_prompt_tone(PLAYER_URI_TYPE_VFS, argv[2], strlen(argv[2]));
}

static void cli_dialog_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc < 2) {
        cli_dialog_help();
        return;
    }

    if (os_strcmp(argv[1], "dump") == 0) {
        cli_dialog_dump_cmd(pcWriteBuffer, xWriteBufferLen, argc, argv);
    } else if (os_strcmp(argv[1], "play") == 0) {
        cli_dialog_play_cmd(pcWriteBuffer, xWriteBufferLen, argc, argv);
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_dialog_help();
    } else {
        CLI_LOGE("Unknown command: %s\r\n", argv[1]);
        cli_dialog_help();
    }
}

static const struct cli_command dialog_cmd[] = {
    {"dialog", "dialog module commands", cli_dialog_cmd},
};

int cli_app_dialog_init(void)
{
    return cli_register_commands(dialog_cmd, sizeof(dialog_cmd) / sizeof(struct cli_command));
}