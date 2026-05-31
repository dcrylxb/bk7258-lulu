#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "bk_cli.h"
#include "bk_posix.h"
#include "driver/flash_partition.h"

#include "common.h"
#include "bk_app_audio.h"

#define VOICE_MOUNT_PATH "/"
#define VOICE_FILE       "/record_test_16k_20ms.opus"

static bool g_voice_initialized = false;
static bool g_voice_started = false;
static int g_voice_fd = -1;

#if CONFIG_FATFS_SDCARD
static int _fs_mount(void)
{
    struct bk_fatfs_partition partition;
    char *fs_name = NULL;
    int ret;

    fs_name = "fatfs";
    partition.part_type = FATFS_DEVICE;
    partition.part_dev.device_name = FATFS_DEV_SDCARD;
    partition.mount_path = VOICE_MOUNT_PATH;

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#else
static int _fs_mount(void)
{
    struct bk_little_fs_partition partition;
    char *fs_name = NULL;
    bk_logic_partition_t *pt = NULL;
    
    pt = bk_flash_partition_get_info(BK_PARTITION_USR_CONFIG);
    if (pt == NULL) {
        CLI_LOGE("usr_config partition not found\n");
        return BK_FAIL;
    }

    fs_name = "littlefs";
    partition.part_type = LFS_FLASH;
    partition.part_flash.start_addr = pt->partition_start_addr;
    partition.part_flash.size = pt->partition_length;
    partition.mount_path = VOICE_MOUNT_PATH;

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#endif

static int _voice_fs_init(void)
{
    int ret = BK_FAIL;
    
    do {
        ret = _fs_mount();
        if (BK_OK != ret) {
            CLI_LOGE("mount fail, ret: %d\n", ret);
            break;
        }

    } while(0);

    return ret;
}

static int _voice_fs_deinit(void)
{
    int ret = BK_FAIL;
    
    do {
        ret = umount(VOICE_MOUNT_PATH);
        if (BK_OK != ret) {
            CLI_LOGE("umount fail, ret: %d\n", ret);
            break;
        }

    } while(0);

    return ret;
}

static int voice_mic_callback(unsigned char *data, unsigned int len, void *args)
{
    int ret = 0;
    uint16_t frame_len = len;

    if (g_voice_started && g_voice_fd >= 0) {
        // 写入2字节的长度
        ret = write(g_voice_fd, &frame_len, sizeof(frame_len));
        if (ret < 0) {
            CLI_LOGE("write length failed\n");
            return ret;
        }
        
        // 写入数据
        ret = write(g_voice_fd, data, len);
        if (ret < 0) {
            CLI_LOGE("write voice data failed\n");
        }
    }
    
    return 0;
}

static void cli_voice_help(void)
{
    CLI_RAW_LOGI("\r\napp_voice [command]\n");
    CLI_RAW_LOGI("  Voice service commands.\n");
    CLI_RAW_LOGI("  Available commands:\n");
    CLI_RAW_LOGI("    init    - Initialize voice service\n");
    CLI_RAW_LOGI("    start   - Start record\n");
    CLI_RAW_LOGI("    stop    - Stop record\n");
    CLI_RAW_LOGI("    play    - Play with file [file_path]\n");
    CLI_RAW_LOGI("    volume  - Set volume (0-100)\n");
    CLI_RAW_LOGI("    deinit  - Deinitialize voice service\n");
    CLI_RAW_LOGI("    help    - Show this help message\n");
}

static void cli_voice_init_cmd(void)
{
    if (g_voice_initialized) {
        CLI_LOGW("Voice service already initialized\n");
        return;
    }

    if (BK_OK != _voice_fs_init()) {
        CLI_LOGE("voice file system init failed\n");
        return;
    }

    app_audio_config_t cfg = DEFAULT_APP_AUDIO_CONFIG();
    cfg.pa_ctrl_gpio = SPEAKER_PA_GPIO;
    cfg.pa_on_level = 1;
    cfg.enc_type = AUDIO_ENC_TYPE_OPUS;
    cfg.dec_type = AUDIO_DEC_TYPE_OPUS;
    cfg.mic_rx_cb = voice_mic_callback;

    if (BK_OK != bk_app_audio_init(&cfg)) {
        CLI_LOGE("app voice init failed\n");
        _voice_fs_deinit();
        return;
    }

    // 关闭 PA
    bk_app_audio_pa_control(false);

    g_voice_initialized = true;
}

static void cli_voice_deinit_cmd(void)
{
    if (!g_voice_initialized) {
        CLI_LOGW("app voice not initialized\n");
        return;
    }

    _voice_fs_deinit();

    if (BK_OK != bk_app_audio_deinit()) {
        CLI_LOGE("app voice deinit failed\n");
        return;
    }

    g_voice_initialized = false;
}

static void cli_voice_start_cmd(void)
{
    if (!g_voice_initialized) {
        CLI_LOGW("app voice not initialized\n");
        return;
    }

    if (g_voice_started) {
        CLI_LOGI("voice already started\n");
        return;
    }
    
    g_voice_fd = open(VOICE_FILE, O_RDWR | O_CREAT | O_TRUNC);
    if (g_voice_fd < 0) {
        CLI_LOGE("create voice file failed\n");
        return;
    }

    g_voice_started = true;
    CLI_LOGI("voice start success\n");
}

static void cli_voice_stop_cmd(void)
{
    if (!g_voice_started) {
        CLI_LOGI("voice not started\n");
        return;
    }
    
    g_voice_started = false;
    
    if (g_voice_fd >= 0) {
        close(g_voice_fd);
        g_voice_fd = -1;
    }
    
    CLI_LOGI("voice stop success\n");
}

static void cli_voice_play_cmd(char *file)
{
    int fd = -1;
    uint8_t *buffer = NULL;
    uint32_t file_size = 0;
    int32_t read_size = 0;
    struct stat st;
    uint32_t loc = 0;
    uint16_t data_len = 0;
    const char *play_file = NULL;
    
    if (file == NULL) {
        play_file = VOICE_FILE;
    } else {
        play_file = file;
    }
    
    if (stat(play_file, &st) != 0) {
        CLI_LOGE("play file %s stat failed\n", play_file);
        return;
    }
    file_size = st.st_size;
    CLI_LOGD("play file size: %d\n", file_size);
    
    fd = open(play_file, O_RDONLY);
    if (fd < 0) {
        CLI_LOGE("open play file %s failed\n", play_file);
        return;
    }
    
    buffer = (uint8_t *)os_malloc(512);
    if (!buffer) {
        CLI_LOGE("malloc buffer failed\n");
        close(fd);
        return;
    }
    
    // 打开 PA
    bk_app_audio_pa_control(true);
    rtos_delay_milliseconds(100);

    while (loc < file_size) {
        // 读取2字节的长度
        read_size = read(fd, &data_len, sizeof(data_len));
        if (read_size != sizeof(data_len)) {
            CLI_LOGE("read length failed or end of file\n");
            break;
        }
        CLI_LOGD("opus frame len: %d\n", data_len);
        
        // 读取数据
        read_size = read(fd, buffer, data_len);
        if (read_size != data_len) {
            CLI_LOGE("read data failed or end of file\n");
            break;
        }
        
        bk_app_audio_write_data(buffer, data_len);
        rtos_delay_milliseconds(20);
        
        loc += sizeof(data_len) + data_len;
    }

    // 关闭 PA
    bk_app_audio_pa_control(false);
    
    close(fd);
    os_free(buffer);
}

static void cli_voice_volume_cmd(char *volume_str)
{
    uint8_t volume;
    
    if (!volume_str) {
        CLI_LOGE("volume parameter is missing\n");
        return;
    }
    
    volume = (uint8_t)os_strtoul(volume_str, NULL, 10);
    if (volume > 100) {
        CLI_LOGE("volume %d is out of range (0-100)\n", volume);
        return;
    }
    
    if (BK_OK != bk_app_audio_set_volume(volume)) {
        CLI_LOGE("set volume failed\n");
        return;
    }
    
    CLI_LOGI("volume set to %d\n", volume);
}

static void cli_voice_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if ((argc == 2) && (!os_strncmp(argv[1], "help", 4))) {
        cli_voice_help();
        return;
    }

    if (argc < 2) {
        CLI_LOGW("invalid argc number\n");
        cli_voice_help();
        return;
    }

    if (os_strcmp(argv[1], "init") == 0) {
        cli_voice_init_cmd();
    } else if (os_strcmp(argv[1], "deinit") == 0) {
        cli_voice_deinit_cmd();
    } else if (os_strcmp(argv[1], "start") == 0) {
        cli_voice_start_cmd();
    } else if (os_strcmp(argv[1], "stop") == 0) {
        cli_voice_stop_cmd();
    } else if (os_strcmp(argv[1], "play") == 0) {
        if (argc >= 3) {
            cli_voice_play_cmd(argv[2]);
        } else {
            cli_voice_play_cmd(NULL);
        }
    } else if (os_strcmp(argv[1], "volume") == 0) {
        if (argc < 3) {
            CLI_LOGW("volume command requires a parameter (0-100)\n");
            cli_voice_help();
            return;
        }
        cli_voice_volume_cmd(argv[2]);
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_voice_help();
    } else {
        CLI_LOGW("invalid parameter: %s\n", argv[1]);
        cli_voice_help();
        return;
    }
}

#define VOICE_CMD_CNT (sizeof(s_voice_commands) / sizeof(struct cli_command))
static const struct cli_command s_voice_commands[] = {
    {"app_voice", "app_voice [command]", cli_voice_cmd},
};

int cli_bk_app_audio_init(void)
{
    return cli_register_commands(s_voice_commands, VOICE_CMD_CNT);
}