#include <stdbool.h>

#include <os/os.h>
#include <os/str.h>
#include "cli.h"
#include "app_ui.h"
#include "common.h"
#include "eye_resource_verify.h"

static const char *s_cli_eye_files[] = {
    EYE_AVI_IDLE_NAME,
    EYE_AVI_LISTEN_NAME,
    EYE_AVI_THINK_NAME,
    EYE_AVI_SPEAK_NAME,
    EYE_AVI_HAPPY_NAME,
    EYE_AVI_CAMERA_NAME,
    EYE_AVI_ERROR_NAME,
    EYE_AVI_SLEEP_NAME,
    EYE_AVI_AFRAID_NAME,
    EYE_AVI_ANGRY_NAME,
    EYE_AVI_BORED_NAME,
    EYE_AVI_CARING_NAME,
    EYE_AVI_DOUBTFUL_NAME,
    EYE_AVI_FROWNING_NAME,
    EYE_AVI_GRIMACING_NAME,
    EYE_AVI_SAD_NAME,
    EYE_AVI_SURPRISED_NAME,
    EYE_AVI_WINKING_NAME,
};

static void cli_app_eye_help(void)
{
    CLI_RAW_LOGI("eye list|play <avi>|carousel [seconds]|stop|verify [hash]\r\n");
    CLI_RAW_LOGI("  eye verify hash\r\n");
}

static void cli_app_eye_list(void)
{
    uint32_t file_count = sizeof(s_cli_eye_files) / sizeof(s_cli_eye_files[0]);

    for (uint32_t i = 0; i < file_count; i++) {
        CLI_RAW_LOGI("  %s\r\n", s_cli_eye_files[i]);
    }
}

static void cli_app_eye_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    (void)pcWriteBuffer;
    (void)xWriteBufferLen;

    if (argc < 2 || argv == NULL) {
        cli_app_eye_help();
        return;
    }

    if (os_strcmp(argv[1], "list") == 0) {
        cli_app_eye_list();
    } else if (os_strcmp(argv[1], "play") == 0) {
        if (argc != 3) {
            cli_app_eye_help();
            return;
        }
        app_ui_eye_carousel_stop();
        if (app_ui_eye_debug_play(argv[2]) != BK_OK) {
            CLI_LOGE("eye play failed: %s\r\n", argv[2]);
        }
    } else if (os_strcmp(argv[1], "carousel") == 0) {
        uint32_t dwell_seconds = 6;

        if (argc >= 3) {
            dwell_seconds = os_strtoul(argv[2], NULL, 10);
        }

        if (app_ui_eye_carousel_start(dwell_seconds) != BK_OK) {
            CLI_LOGE("eye carousel start failed\r\n");
        }
    } else if (os_strcmp(argv[1], "stop") == 0) {
        app_ui_eye_carousel_stop();
    } else if (os_strcmp(argv[1], "verify") == 0) {
        if (argc == 3 && os_strcmp(argv[2], "hash") == 0) {
            if (eye_resource_verify_sf0_manifest(true) == BK_OK) {
                CLI_LOGI("eye verify ok\r\n");
            } else {
                CLI_LOGE("eye verify failed\r\n");
            }
        } else if (argc != 2) {
            cli_app_eye_help();
            return;
        } else {
            if (eye_resource_verify_sf0_manifest(false) == BK_OK) {
                CLI_LOGI("eye verify ok\r\n");
            } else {
                CLI_LOGE("eye verify failed\r\n");
            }
        }
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_app_eye_help();
    } else {
        CLI_LOGE("Unknown eye command: %s\r\n", argv[1]);
        cli_app_eye_help();
    }
}

static const struct cli_command s_eye_commands[] = {
    {"eye", "eye list|play <avi>|carousel [seconds]|stop|verify [hash]", cli_app_eye_cmd},
};

int cli_app_eye_init(void)
{
    return cli_register_commands(s_eye_commands, sizeof(s_eye_commands) / sizeof(struct cli_command));
}
