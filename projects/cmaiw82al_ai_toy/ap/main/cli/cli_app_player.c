#include <string.h>
#include "components/log.h"
#include "cli.h"
#include "bk_app_audio.h"
#include "app_audio_player.h"

static void cli_player_help(void)
{
    CLI_RAW_LOGI("ap_cmd app_player [command] [options]\r\n");
    CLI_RAW_LOGI("  start                - Start playback\r\n");
    CLI_RAW_LOGI("  stop                 - Stop playback\r\n");
    CLI_RAW_LOGI("  pause                - Pause playback\r\n");
    CLI_RAW_LOGI("  resume               - Resume playback\r\n");
    CLI_RAW_LOGI("  next                 - Play next song\r\n");
    CLI_RAW_LOGI("  prev                 - Play previous song\r\n");
    CLI_RAW_LOGI("  add <name> <uri>     - Add music to playlist\r\n");
    CLI_RAW_LOGI("  clear                - Clear playlist\r\n");
    CLI_RAW_LOGI("  seek <seconds>       - Seek to position\r\n");
    CLI_RAW_LOGI("  volume <0-100>       - Set volume\r\n");
}

static void cli_player_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    bk_err_t ret;

    if (argc < 2) {
        cli_player_help();
        return;
    }

    if (os_strcmp(argv[1], "start") == 0) {
        ret = app_audio_player_start();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Player started\r\n");
        } else {
            CLI_LOGE("player start fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "stop") == 0) {
        ret = app_audio_player_stop();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Player stopped\r\n");
        } else {
            CLI_LOGE("player stop fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "pause") == 0) {
        ret = app_audio_player_pause();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Player paused\r\n");
        } else {
            CLI_LOGE("player pause fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "resume") == 0) {
        ret = app_audio_player_resume();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Player resumed\r\n");
        } else {
            CLI_LOGE("player resume fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "next") == 0) {
        ret = app_audio_player_next();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Playing next song\r\n");
        } else {
            CLI_LOGE("player next fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "prev") == 0) {
        ret = app_audio_player_prev();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Playing previous song\r\n");
        } else {
            CLI_LOGE("player prev fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "add") == 0) {
        if (argc < 4) {
            CLI_LOGE("Usage: player add <name> <uri>\r\n");
            return;
        }
        ret = app_audio_player_add_music(argv[2], argv[3]);
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Added music: %s -> %s\r\n", argv[2], argv[3]);
        } else {
            CLI_LOGE("player add music fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "clear") == 0) {
        ret = app_audio_player_clear_music_list();
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Playlist cleared\r\n");
        } else {
            CLI_LOGE("player clear list fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "seek") == 0) {
        if (argc < 3) {
            CLI_LOGE("Usage: player seek <seconds>\r\n");
            return;
        }
        int seconds = atoi(argv[2]);
        ret = app_audio_player_seek(seconds);
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Seeked to %d seconds\r\n", seconds);
        } else {
            CLI_LOGE("player seek fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "volume") == 0) {
        if (argc < 3) {
            CLI_LOGE("Usage: player volume <0-100>\r\n");
            return;
        }
        int volume = atoi(argv[2]);
        ret = app_audio_player_set_volume(volume);
        if (ret == BK_OK) {
            CLI_RAW_LOGI("Volume set to %d\r\n", volume);
        } else {
            CLI_LOGE("player set volume fail, ret: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_player_help();
    } else {
        CLI_LOGE("Unknown command: %s\r\n", argv[1]);
        cli_player_help();
    }
}

static const struct cli_command player_cmd[] = {
    {"app_player", "audio player commands", cli_player_cmd},
};

int cli_app_player_init(void)
{
    return cli_register_commands(player_cmd, sizeof(player_cmd) / sizeof(struct cli_command));
}
