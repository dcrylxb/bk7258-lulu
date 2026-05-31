#include <components/webclient.h>
#include <components/system.h>
#include "driver/flash_partition.h"
#include <driver/spi_flash.h>
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>
#include <sys/stat.h>
#include "modules/ota.h"
#include "bk_posix.h"
#include "cli.h"
#include "app_vfs.h"
#include "bk_app_audio.h"
#include "common.h"
#include "eye_resource_verify.h"
#if CONFIG_LCD
#include "app_ui.h"
#endif

#define CMAIW82AL_UPDATE_SPI_FLASH_ID      SPI_ID_0
#define CMAIW82AL_UPDATE_SPI_FLASH_SIZE 0xF00000
#define CMAIW82AL_UPDATE_RX_BUF_SIZE       4096
#define CMAIW82AL_UPDATE_HTTP_HEADER_SIZE  1024
#define CMAIW82AL_UPDATE_TASK_STACK        6144
#define CMAIW82AL_UPDATE_TASK_PRIO         BEKEN_APPLICATION_PRIORITY
#define CMAIW82AL_UPDATE_FLASH_HEADER_LEN  32
#define CMAIW82AL_UPDATE_MAX_DIR_ENTRIES   24
#define CMAIW82AL_UPDATE_FNV32_OFFSET      2166136261u
#define CMAIW82AL_UPDATE_FNV32_PRIME       16777619u
#define CMAIW82AL_UPDATE_FLASH_SECTOR_SIZE 4096
#define CMAIW82AL_UPDATE_VERIFY_BLOCK_SIZE 4096
#define CMAIW82AL_UPDATE_VERIFY_LOG_LIMIT  8
#define CMAIW82AL_UPDATE_WRITE_VERIFY_RETRY 2
#define CMAIW82AL_UPDATE_SECTOR_REWRITE_RETRY 2
#define CMAIW82AL_UPDATE_READ_VERIFY_RETRY 2

typedef struct {
    char *url;
    bool is_eye_update;
    bool is_ota_update;
    bool is_if0_update;
} cmaiw82al_update_task_arg_t;

static volatile bool s_update_flash_running = false;
static const char *s_default_eye_url = "http://192.168.31.140/sf0_bk_avi_320x160_15m.bin";

static void cli_app_update_help(void)
{
    CLI_LOGI("usage: update flash <http-url>\r\n");
    CLI_LOGI("usage: update eyes <http-url>\r\n");
    CLI_LOGI("usage: update ota <http-url>\r\n");
    CLI_LOGI("usage: update if0 <http-url>\r\n");
    CLI_LOGI("usage: update eyes\r\n");
    CLI_LOGI("example: update flash http://192.168.43.130/sf0_gc9d01_320x160_test_15m.bin\r\n");
    CLI_LOGI("example: update eyes http://192.168.31.140/sf0_bk_avi_320x160_15m.bin\r\n");
    CLI_LOGI("example: update ota http://192.168.31.117:8000/app_pack.rbl\r\n");
    CLI_LOGI("example: update if0 http://192.168.31.117:8000/bk_0x6f9000.bin\r\n");
}

static bool cli_app_update_url_is_http(const char *url)
{
    return url != NULL && os_strncmp(url, "http://", 7) == 0;
}

static uint32_t cli_app_update_hash_update(uint32_t hash, const uint8_t *data, uint32_t len)
{
    if (data == NULL) {
        return hash;
    }

    for (uint32_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= CMAIW82AL_UPDATE_FNV32_PRIME;
    }

    return hash;
}

static bk_err_t cli_app_update_record_expected_blocks(uint32_t *expected_block_hashes,
                                                      uint32_t expected_block_count,
                                                      uint32_t *block_index,
                                                      uint32_t *block_hash,
                                                      uint32_t *block_len,
                                                      const uint8_t *data,
                                                      uint32_t len)
{
    uint32_t offset = 0;

    if (expected_block_hashes == NULL || block_index == NULL ||
        block_hash == NULL || block_len == NULL || data == NULL) {
        return BK_ERR_PARAM;
    }

    while (offset < len) {
        uint32_t remaining = len - offset;
        uint32_t block_left = CMAIW82AL_UPDATE_VERIFY_BLOCK_SIZE - *block_len;
        uint32_t take = remaining < block_left ? remaining : block_left;

        *block_hash = cli_app_update_hash_update(*block_hash, data + offset, take);
        *block_len += take;
        offset += take;

        if (*block_len == CMAIW82AL_UPDATE_VERIFY_BLOCK_SIZE) {
            if (*block_index >= expected_block_count) {
                CLI_LOGE("EYES_UPDATE expected block overflow index=%u count=%u\r\n",
                         *block_index,
                         expected_block_count);
                return BK_FAIL;
            }

            expected_block_hashes[*block_index] = *block_hash;
            (*block_index)++;
            *block_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
            *block_len = 0;
        }
    }

    return BK_OK;
}

static bk_err_t cli_app_update_finalize_expected_blocks(uint32_t *expected_block_hashes,
                                                        uint32_t expected_block_count,
                                                        uint32_t *block_index,
                                                        uint32_t *block_hash,
                                                        uint32_t *block_len)
{
    if (expected_block_hashes == NULL || block_index == NULL ||
        block_hash == NULL || block_len == NULL) {
        return BK_ERR_PARAM;
    }

    if (*block_len > 0) {
        if (*block_index >= expected_block_count) {
            CLI_LOGE("EYES_UPDATE expected tail block overflow index=%u count=%u\r\n",
                     *block_index,
                     expected_block_count);
            return BK_FAIL;
        }
        expected_block_hashes[*block_index] = *block_hash;
        (*block_index)++;
        *block_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
        *block_len = 0;
    }

    if (*block_index != expected_block_count) {
        CLI_LOGE("EYES_UPDATE expected block count mismatch actual=%u expected=%u\r\n",
                 *block_index,
                 expected_block_count);
        return BK_FAIL;
    }

    return BK_OK;
}

static uint32_t cli_app_update_hash_block(const uint8_t *data, uint32_t len)
{
    return cli_app_update_hash_update(CMAIW82AL_UPDATE_FNV32_OFFSET, data, len);
}

static bk_err_t cli_app_update_read_block_hash(uint32_t offset,
                                               uint8_t *buffer,
                                               uint32_t len,
                                               uint32_t *hash)
{
    bk_err_t ret = BK_OK;

    if (buffer == NULL || hash == NULL || len == 0) {
        return BK_ERR_PARAM;
    }

    ret = bk_spi_flash_read(CMAIW82AL_UPDATE_SPI_FLASH_ID, offset, buffer, len);
    if (ret != BK_OK) {
        return ret;
    }

    *hash = cli_app_update_hash_block(buffer, len);
    return BK_OK;
}

static bool cli_app_update_read_expected_twice(uint32_t offset,
                                               uint8_t *buffer,
                                               uint32_t len,
                                               uint32_t expected_hash,
                                               const uint8_t *expected,
                                               uint32_t *retry_hash1,
                                               uint32_t *retry_hash2)
{
    uint32_t retry_hashes[CMAIW82AL_UPDATE_READ_VERIFY_RETRY] = {0};
    bool recovered = true;

    if (buffer == NULL || len == 0) {
        return false;
    }

    for (uint32_t retry = 0; retry < CMAIW82AL_UPDATE_READ_VERIFY_RETRY; retry++) {
        bk_err_t ret = cli_app_update_read_block_hash(offset, buffer, len, &retry_hashes[retry]);

        if (ret != BK_OK ||
            retry_hashes[retry] != expected_hash ||
            (expected != NULL && os_memcmp(expected, buffer, len) != 0)) {
            recovered = false;
        }
    }

    if (retry_hash1 != NULL) {
        *retry_hash1 = retry_hashes[0];
    }
    if (retry_hash2 != NULL) {
        *retry_hash2 = retry_hashes[1];
    }

    return recovered;
}

static uint8_t cli_app_update_byte_at(const uint8_t *data, uint32_t len, uint32_t index)
{
    if (data == NULL || index >= len) {
        return 0;
    }

    return data[index];
}

static bool cli_app_update_can_rewrite_without_erase(const uint8_t *expected,
                                                     const uint8_t *actual,
                                                     uint32_t len)
{
    if (expected == NULL || actual == NULL || len == 0) {
        return false;
    }

    for (uint32_t i = 0; i < len; i++) {
        if ((actual[i] & expected[i]) != expected[i]) {
            return false;
        }
    }

    return true;
}

static bool cli_app_update_can_erase_rewrite_sector(uint32_t offset, uint32_t len)
{
    return (offset & (CMAIW82AL_UPDATE_FLASH_SECTOR_SIZE - 1)) == 0 &&
           len == CMAIW82AL_UPDATE_FLASH_SECTOR_SIZE;
}

static bk_err_t cli_app_update_verify_written_chunk(uint32_t offset,
                                                    const uint8_t *expected,
                                                    uint8_t *read_buffer,
                                                    uint32_t len)
{
    uint32_t expected_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    uint32_t actual_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    bk_err_t ret = BK_OK;
    uint32_t retry = 0;
    uint32_t sector_retry = 0;
    uint32_t read_retry = 0;

    if (expected == NULL || read_buffer == NULL || len == 0) {
        return BK_ERR_PARAM;
    }

    do {
        ret = bk_spi_flash_read(CMAIW82AL_UPDATE_SPI_FLASH_ID, offset, read_buffer, len);
        if (ret != BK_OK) {
            CLI_LOGE("EYES_UPDATE write verify read failed offset=0x%06x len=%u ret=%d\r\n",
                     offset,
                     len,
                     ret);
            return ret;
        }

        expected_hash = cli_app_update_hash_update(expected_hash, expected, len);
        actual_hash = cli_app_update_hash_update(actual_hash, read_buffer, len);
        if (actual_hash == expected_hash && os_memcmp(expected, read_buffer, len) == 0) {
            return BK_OK;
        }

        {
            uint32_t retry_hash1 = 0;
            uint32_t retry_hash2 = 0;

            if (cli_app_update_read_expected_twice(offset,
                                                   read_buffer,
                                                   len,
                                                   expected_hash,
                                                   expected,
                                                   &retry_hash1,
                                                   &retry_hash2)) {
                CLI_LOGW("EYES_UPDATE write verify read retry recovered offset=0x%06x len=%u retry_hash1=0x%08x retry_hash2=0x%08x\r\n",
                         offset,
                         len,
                         retry_hash1,
                         retry_hash2);
                return BK_OK;
            }

            if (retry_hash1 != retry_hash2 && read_retry < CMAIW82AL_UPDATE_READ_VERIFY_RETRY) {
                CLI_LOGW("EYES_UPDATE write verify unstable read offset=0x%06x len=%u retry=%u actual_hash=0x%08x retry_hash1=0x%08x retry_hash2=0x%08x\r\n",
                         offset,
                         len,
                         read_retry + 1,
                         actual_hash,
                         retry_hash1,
                         retry_hash2);
                read_retry++;
                expected_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
                actual_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
                continue;
            }
        }

        if (cli_app_update_can_rewrite_without_erase(expected, read_buffer, len) &&
            retry < CMAIW82AL_UPDATE_WRITE_VERIFY_RETRY) {
            ret = bk_spi_flash_write(CMAIW82AL_UPDATE_SPI_FLASH_ID, offset, expected, len);
            CLI_LOGW("EYES_UPDATE write verify rewrite offset=0x%06x len=%u retry=%u rewrite_ret=%d\r\n",
                     offset,
                     len,
                     retry + 1,
                     ret);
            if (ret != BK_OK) {
                return ret;
            }
            retry++;
        } else if ((retry >= CMAIW82AL_UPDATE_WRITE_VERIFY_RETRY ||
                    !cli_app_update_can_rewrite_without_erase(expected, read_buffer, len)) &&
                   cli_app_update_can_erase_rewrite_sector(offset, len)) {
            bk_err_t erase_ret = BK_FAIL;
            bk_err_t write_ret = BK_FAIL;

            if (sector_retry >= CMAIW82AL_UPDATE_SECTOR_REWRITE_RETRY) {
                break;
            }

            erase_ret = bk_spi_flash_erase(CMAIW82AL_UPDATE_SPI_FLASH_ID, offset, len);
            if (erase_ret == BK_OK) {
                write_ret = bk_spi_flash_write(CMAIW82AL_UPDATE_SPI_FLASH_ID, offset, expected, len);
            }
            CLI_LOGW("EYES_UPDATE write verify sector rewrite offset=0x%06x len=%u retry=%u erase_ret=%d sector_write_ret=%d\r\n",
                     offset,
                     len,
                     sector_retry + 1,
                     erase_ret,
                     write_ret);
            if (erase_ret != BK_OK) {
                return erase_ret;
            }
            if (write_ret != BK_OK) {
                return write_ret;
            }
            sector_retry++;
        } else {
             break;
        }

        expected_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
        actual_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    } while (retry <= CMAIW82AL_UPDATE_WRITE_VERIFY_RETRY ||
             sector_retry <= CMAIW82AL_UPDATE_SECTOR_REWRITE_RETRY);

    if (actual_hash != expected_hash || os_memcmp(expected, read_buffer, len) != 0) {
        uint32_t first_diff = 0;
        uint32_t diff_base = 0;

        while (first_diff < len && expected[first_diff] == read_buffer[first_diff]) {
            first_diff++;
        }

        if (first_diff >= len) {
            first_diff = 0;
        }

        diff_base = first_diff > 4 ? first_diff - 4 : 0;
        if (len > 8 && (diff_base + 8) > len) {
            diff_base = len - 8;
        }

        CLI_LOGE("EYES_UPDATE write verify mismatch offset=0x%06x len=%u expected_hash=0x%08x actual_hash=0x%08x first_diff=0x%06x chunk_diff=0x%04x prefix=%02x %02x %02x %02x read=%02x %02x %02x %02x\r\n",
                 offset,
                 len,
                 expected_hash,
                 actual_hash,
                 offset + first_diff,
                 first_diff,
                 expected[0], len > 1 ? expected[1] : 0, len > 2 ? expected[2] : 0, len > 3 ? expected[3] : 0,
                 read_buffer[0], len > 1 ? read_buffer[1] : 0, len > 2 ? read_buffer[2] : 0, len > 3 ? read_buffer[3] : 0);
        CLI_LOGE("EYES_UPDATE write verify diff_expected=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                 cli_app_update_byte_at(expected, len, diff_base + 0),
                 cli_app_update_byte_at(expected, len, diff_base + 1),
                 cli_app_update_byte_at(expected, len, diff_base + 2),
                 cli_app_update_byte_at(expected, len, diff_base + 3),
                 cli_app_update_byte_at(expected, len, diff_base + 4),
                 cli_app_update_byte_at(expected, len, diff_base + 5),
                 cli_app_update_byte_at(expected, len, diff_base + 6),
                 cli_app_update_byte_at(expected, len, diff_base + 7));
        CLI_LOGE("EYES_UPDATE write verify diff_read=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                 cli_app_update_byte_at(read_buffer, len, diff_base + 0),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 1),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 2),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 3),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 4),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 5),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 6),
                 cli_app_update_byte_at(read_buffer, len, diff_base + 7));
        return BK_FAIL;
    }

    return BK_OK;
}

static bk_err_t cli_app_update_verify_raw_flash_image(uint32_t image_size,
                                                      uint32_t expected_hash,
                                                      const uint32_t *expected_block_hashes,
                                                      uint32_t expected_block_count)
{
    uint8_t *buffer = NULL;
    uint32_t offset = 0;
    uint32_t actual_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    uint32_t block_index = 0;
    uint32_t block_mismatches = 0;
    uint32_t raw_read_recoveries = 0;
    uint32_t logged_mismatches = 0;
    bk_err_t ret = BK_FAIL;

    if (image_size == 0 || image_size > CMAIW82AL_UPDATE_SPI_FLASH_SIZE) {
        CLI_LOGE("EYES_UPDATE raw verify invalid size=%u\r\n", image_size);
        return BK_ERR_PARAM;
    }

    CLI_LOGI("EYES_UPDATE raw verify begin size=%u expected_hash=0x%08x\r\n",
             image_size,
             expected_hash);

    buffer = os_malloc(CMAIW82AL_UPDATE_RX_BUF_SIZE);
    if (buffer == NULL) {
        CLI_LOGE("EYES_UPDATE raw verify malloc failed size=%u\r\n",
                 CMAIW82AL_UPDATE_RX_BUF_SIZE);
        return BK_ERR_NO_MEM;
    }

    while (offset < image_size) {
        uint32_t read_size = image_size - offset;
        uint32_t block_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;

        if (read_size > CMAIW82AL_UPDATE_RX_BUF_SIZE) {
            read_size = CMAIW82AL_UPDATE_RX_BUF_SIZE;
        }

        ret = cli_app_update_read_block_hash(offset, buffer, read_size, &block_hash);
        if (ret != BK_OK) {
            CLI_LOGE("EYES_UPDATE raw verify read failed offset=%u len=%u ret=%d\r\n",
                     offset,
                     read_size,
                     ret);
            break;
        }

        if (expected_block_hashes != NULL && block_index < expected_block_count &&
            block_hash != expected_block_hashes[block_index]) {
            uint32_t retry_hash1 = 0;
            uint32_t retry_hash2 = 0;

            if (cli_app_update_read_expected_twice(offset,
                                                   buffer,
                                                   read_size,
                                                   expected_block_hashes[block_index],
                                                   NULL,
                                                   &retry_hash1,
                                                   &retry_hash2)) {
                raw_read_recoveries++;
                block_hash = retry_hash2;
                if (logged_mismatches < CMAIW82AL_UPDATE_VERIFY_LOG_LIMIT) {
                    CLI_LOGW("EYES_UPDATE raw verify read retry recovered index=%u offset=0x%06x len=%u actual_hash=0x%08x retry_hash1=0x%08x retry_hash2=0x%08x\r\n",
                             block_index,
                             offset,
                             read_size,
                             block_hash,
                             retry_hash1,
                             retry_hash2);
                    logged_mismatches++;
                }
            } else {
                block_mismatches++;
                block_hash = retry_hash2;

                if (logged_mismatches < CMAIW82AL_UPDATE_VERIFY_LOG_LIMIT) {
                    CLI_LOGE("EYES_UPDATE raw verify block mismatch index=%u offset=0x%06x len=%u expected_hash=0x%08x actual_hash=0x%08x retry_hash1=0x%08x retry_hash2=0x%08x\r\n",
                             block_index,
                             offset,
                             read_size,
                             expected_block_hashes[block_index],
                             block_hash,
                             retry_hash1,
                             retry_hash2);
                    logged_mismatches++;
                }
            }
        }

        actual_hash = cli_app_update_hash_update(actual_hash, buffer, read_size);
        offset += read_size;
        block_index++;
    }

    if (offset == image_size && actual_hash == expected_hash) {
        ret = BK_OK;
    } else if (ret == BK_OK) {
        ret = BK_FAIL;
    }

    CLI_LOGI("EYES_UPDATE raw verify blocks total=%u mismatches=%u recovered=%u\r\n",
             block_index,
             block_mismatches,
             raw_read_recoveries);
    CLI_LOGI("EYES_UPDATE raw verify hash size=%u expected_hash=0x%08x actual_hash=0x%08x ret=%d\r\n",
             offset,
             expected_hash,
             actual_hash,
             ret);

    os_free(buffer);
    return ret;
}

static void cli_app_update_log_flash_header(void)
{
    uint8_t header[CMAIW82AL_UPDATE_FLASH_HEADER_LEN] = {0};
    bk_err_t ret = bk_spi_flash_read(CMAIW82AL_UPDATE_SPI_FLASH_ID, 0, header, sizeof(header));

    if (ret != BK_OK) {
        CLI_LOGE("EYES_UPDATE flash header read fail, ret=%d\r\n", ret);
        return;
    }

    CLI_LOGI("EYES_UPDATE flash[0..%u]=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
             (unsigned)(sizeof(header) - 1),
             header[0], header[1], header[2], header[3],
             header[4], header[5], header[6], header[7],
             header[8], header[9], header[10], header[11],
             header[12], header[13], header[14], header[15],
             header[16], header[17], header[18], header[19],
             header[20], header[21], header[22], header[23],
             header[24], header[25], header[26], header[27],
             header[28], header[29], header[30], header[31]);
}

static void cli_app_update_log_sf0_dir(void)
{
    DIR *dir = opendir("/sf0");
    struct dirent *entry = NULL;
    uint32_t count = 0;

    if (dir == NULL) {
        CLI_LOGE("EYES_UPDATE /sf0 opendir fail\r\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL && count < CMAIW82AL_UPDATE_MAX_DIR_ENTRIES) {
        CLI_LOGI("EYES_UPDATE /sf0 dir entry[%u] name=%s type=%u\r\n",
                 count,
                 entry->d_name,
                 entry->d_type);
        count++;
    }

    CLI_LOGI("EYES_UPDATE /sf0 dir entries logged=%u\r\n", count);
    closedir(dir);
}

static void cli_app_update_log_eye_file_stat(void)
{
    struct stat st = {0};
    int stat_ret = stat(EYE_AVI_FILE, &st);

    if (stat_ret == 0) {
        CLI_LOGI("EYES_UPDATE /sf0 stat path=%s size=%ld\r\n",
                 EYE_AVI_FILE,
                 (long)st.st_size);
    } else {
        CLI_LOGE("EYES_UPDATE /sf0 stat fail path=%s ret=%d\r\n",
                 EYE_AVI_FILE,
                 stat_ret);
        cli_app_update_log_sf0_dir();
        cli_app_update_log_flash_header();
    }
}

static bk_err_t cli_app_update_download_flash_image(const char *url, bool is_eye_update)
{
    struct webclient_session *session = NULL;
    uint8_t *buffer = NULL;
    uint8_t *verify_buffer = NULL;
    uint32_t *expected_block_hashes = NULL;
    uint32_t expected_block_count = 0;
    uint32_t recorded_block_count = 0;
    uint32_t expected_block_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    uint32_t expected_block_len = 0;
    int status = 0;
    int content_length = 0;
    int bytes_read = 0;
    uint32_t write_offset = 0;
    uint32_t download_hash = CMAIW82AL_UPDATE_FNV32_OFFSET;
    bk_err_t ret = BK_FAIL;

    if (is_eye_update) {
        CLI_LOGI("EYES_UPDATE begin: %s\r\n", url);
#if CONFIG_LCD
        ret = app_ui_prepare_eye_resource_update();
        if (ret != BK_OK) {
            CLI_LOGE("EYES_UPDATE prepare eye resource failed, ret=%d\r\n", ret);
            return ret;
        }
#else
        CLI_LOGI("EYES_UPDATE LCD disabled, skip AVI player stop\r\n");
#endif
    }

    CLI_LOGI("update flash begin: %s\r\n", url);
    CLI_LOGI("target: /sf0 external SPI%d littlefs image, erase size=0x%x\r\n",
        CMAIW82AL_UPDATE_SPI_FLASH_ID, CMAIW82AL_UPDATE_SPI_FLASH_SIZE);

    app_vfs_unmount_sf0();

    ret = bk_spi_flash_init(CMAIW82AL_UPDATE_SPI_FLASH_ID);
    if (ret != BK_OK) {
        CLI_LOGE("spi flash init failed, ret=%d\r\n", ret);
        goto out_remount;
    }

    session = webclient_session_create(CMAIW82AL_UPDATE_HTTP_HEADER_SIZE);
    if (session == NULL) {
        CLI_LOGE("webclient session create failed\r\n");
        goto out_remount;
    }

    buffer = os_malloc(CMAIW82AL_UPDATE_RX_BUF_SIZE);
    if (buffer == NULL) {
        CLI_LOGE("malloc rx buffer failed, size=%u\r\n", CMAIW82AL_UPDATE_RX_BUF_SIZE);
        goto out_remount;
    }

    webclient_header_fields_add(session, "Accept: application/octet-stream\r\n");
    webclient_header_fields_add(session, "Connection: close\r\n");
    status = webclient_get(session, url);
    if (status != 200) {
        CLI_LOGE("http get failed, status=%d\r\n", status);
        goto out_remount;
    }

    content_length = webclient_content_length_get(session);
    CLI_LOGI("http content_length=%d\r\n", content_length);
    if (is_eye_update) {
        CLI_LOGI("EYES_UPDATE content_length=%d\r\n", content_length);
    }
    if (content_length <= 0) {
        CLI_LOGE("invalid content length: %d\r\n", content_length);
        goto out_remount;
    }
    if ((uint32_t)content_length > CMAIW82AL_UPDATE_SPI_FLASH_SIZE) {
        CLI_LOGE("image too large: %d > %u\r\n", content_length, CMAIW82AL_UPDATE_SPI_FLASH_SIZE);
        goto out_remount;
    }

    if (is_eye_update) {
        expected_block_count = ((uint32_t)content_length + CMAIW82AL_UPDATE_VERIFY_BLOCK_SIZE - 1) /
                               CMAIW82AL_UPDATE_VERIFY_BLOCK_SIZE;
        expected_block_hashes = os_zalloc(expected_block_count * sizeof(uint32_t));
        if (expected_block_hashes == NULL) {
            CLI_LOGE("EYES_UPDATE expected block hash malloc failed count=%u\r\n",
                     expected_block_count);
            goto out_remount;
        }

        verify_buffer = os_malloc(CMAIW82AL_UPDATE_RX_BUF_SIZE);
        if (verify_buffer == NULL) {
            CLI_LOGE("EYES_UPDATE verify buffer malloc failed size=%u\r\n",
                     CMAIW82AL_UPDATE_RX_BUF_SIZE);
            goto out_remount;
        }
    }

    CLI_LOGI("erase external SPI flash...\r\n");
    ret = bk_spi_flash_erase(CMAIW82AL_UPDATE_SPI_FLASH_ID, 0, CMAIW82AL_UPDATE_SPI_FLASH_SIZE);
    if (ret != BK_OK) {
        CLI_LOGE("spi flash erase failed, ret=%d\r\n", ret);
        goto out_remount;
    }

    do {
        bytes_read = webclient_read(session, buffer, CMAIW82AL_UPDATE_RX_BUF_SIZE);
        if (bytes_read < 0) {
            CLI_LOGE("http read failed, ret=%d\r\n", bytes_read);
            goto out_remount;
        }
        if (bytes_read == 0) {
            break;
        }
        if ((write_offset + (uint32_t)bytes_read) > CMAIW82AL_UPDATE_SPI_FLASH_SIZE) {
            CLI_LOGE("download overflow at offset=%u read=%d\r\n", write_offset, bytes_read);
            goto out_remount;
        }

        if (is_eye_update) {
            ret = cli_app_update_record_expected_blocks(expected_block_hashes,
                                                        expected_block_count,
                                                        &recorded_block_count,
                                                        &expected_block_hash,
                                                        &expected_block_len,
                                                        buffer,
                                                        (uint32_t)bytes_read);
            if (ret != BK_OK) {
                goto out_remount;
            }
        }

        ret = bk_spi_flash_write(CMAIW82AL_UPDATE_SPI_FLASH_ID, write_offset, buffer, bytes_read);
        if (ret != BK_OK) {
            CLI_LOGE("spi flash write failed, offset=%u len=%d ret=%d\r\n",
                write_offset, bytes_read, ret);
            goto out_remount;
        }

        if (is_eye_update) {
            ret = cli_app_update_verify_written_chunk(write_offset,
                                                      buffer,
                                                      verify_buffer,
                                                      (uint32_t)bytes_read);
            if (ret != BK_OK) {
                goto out_remount;
            }
        }

        download_hash = cli_app_update_hash_update(download_hash, buffer, (uint32_t)bytes_read);
        write_offset += bytes_read;
        if ((write_offset % (256 * 1024)) == 0 || write_offset == (uint32_t)content_length) {
            CLI_LOGI("update flash progress: %u/%d\r\n", write_offset, content_length);
            if (is_eye_update) {
                CLI_LOGI("EYES_UPDATE progress: %u/%d\r\n", write_offset, content_length);
            }
        }
    } while (write_offset < (uint32_t)content_length);

    if (write_offset != (uint32_t)content_length) {
        CLI_LOGE("download size mismatch: wrote=%u expected=%d\r\n", write_offset, content_length);
        goto out_remount;
    }

    if (is_eye_update &&
        cli_app_update_finalize_expected_blocks(expected_block_hashes,
                                                expected_block_count,
                                                &recorded_block_count,
                                                &expected_block_hash,
                                                &expected_block_len) != BK_OK) {
        goto out_remount;
    }

    CLI_LOGI("update flash image written: %u bytes\r\n", write_offset);
    if (is_eye_update) {
        CLI_LOGI("EYES_UPDATE download hash bytes=%u hash=0x%08x\r\n",
                 write_offset,
                 download_hash);
        if (cli_app_update_verify_raw_flash_image(write_offset, download_hash, expected_block_hashes, expected_block_count) != BK_OK) {
            CLI_LOGE("EYES_UPDATE raw flash verify failed\r\n");
            ret = BK_FAIL;
            goto out_remount;
        }
    }
    ret = BK_OK;

out_remount:
    if (session) {
        webclient_close(session);
    }
    if (buffer) {
        os_free(buffer);
    }
    if (verify_buffer) {
        os_free(verify_buffer);
    }
    if (expected_block_hashes) {
        os_free(expected_block_hashes);
    }

    if (app_vfs_mount_sf0() == BK_OK) {
        CLI_LOGI("remount /sf0 success\r\n");
        if (is_eye_update) {
            cli_app_update_log_eye_file_stat();
            if (ret == BK_OK && eye_resource_verify_sf0_manifest(false) != BK_OK) {
                CLI_LOGE("EYES_UPDATE /sf0 manifest verify failed\r\n");
                ret = BK_FAIL;
            }
        }
    } else {
        CLI_LOGE("remount /sf0 failed\r\n");
        if (ret == BK_OK) {
            ret = BK_FAIL;
        }
    }

    if (is_eye_update && ret == BK_OK) {
        CLI_LOGI("EYES_UPDATE skip hot reopen; reboot will load new eye resource\r\n");
    }

    CLI_LOGI("update flash finish, ret=%d\r\n", ret);
    if (is_eye_update) {
        CLI_LOGI("EYES_UPDATE finish, ret=%d\r\n", ret);
    }
    return ret;
}

static bk_err_t cli_app_update_download_if0_image(const char *url)
{
    struct webclient_session *session = NULL;
    uint8_t *buffer = NULL;
    bk_logic_partition_t *partition = NULL;
    uint32_t partition_size = 0;
    int status = 0;
    int content_length = 0;
    int bytes_read = 0;
    uint32_t write_offset = 0;
    bk_err_t ret = BK_FAIL;

    CLI_LOGI("IF0_UPDATE begin: %s\r\n", url);

    partition = bk_flash_partition_get_info(BK_PARTITION_USR_CONFIG);
    if (partition == NULL) {
        CLI_LOGE("IF0_UPDATE usr_config partition not found\r\n");
        return BK_FAIL;
    }
    partition_size = partition->partition_length;

    session = webclient_session_create(CMAIW82AL_UPDATE_HTTP_HEADER_SIZE);
    if (session == NULL) {
        CLI_LOGE("IF0_UPDATE webclient session create failed\r\n");
        goto out;
    }

    buffer = os_malloc(CMAIW82AL_UPDATE_RX_BUF_SIZE);
    if (buffer == NULL) {
        CLI_LOGE("IF0_UPDATE malloc rx buffer failed, size=%u\r\n", CMAIW82AL_UPDATE_RX_BUF_SIZE);
        goto out;
    }

    webclient_header_fields_add(session, "Accept: application/octet-stream\r\n");
    webclient_header_fields_add(session, "Connection: close\r\n");
    status = webclient_get(session, url);
    if (status != 200) {
        CLI_LOGE("IF0_UPDATE http get failed, status=%d\r\n", status);
        goto out;
    }

    content_length = webclient_content_length_get(session);
    CLI_LOGI("IF0_UPDATE content_length=%d\r\n", content_length);
    if (content_length <= 0) {
        CLI_LOGE("IF0_UPDATE invalid content length: %d\r\n", content_length);
        goto out;
    }
    if ((uint32_t)content_length > partition_size) {
        CLI_LOGE("IF0_UPDATE image too large: %d > %u\r\n", content_length, partition_size);
        goto out;
    }

    CLI_LOGI("IF0_UPDATE stop prompt tone before unmount\r\n");
    bk_app_audio_stop_prompt_tone();
    rtos_delay_milliseconds(120);

    app_vfs_unmount_if0();

    CLI_LOGI("IF0_UPDATE erase usr_config partition size=%u\r\n", partition_size);
    ret = bk_flash_partition_erase(BK_PARTITION_USR_CONFIG, 0, partition_size);
    if (ret != BK_OK) {
        CLI_LOGE("IF0_UPDATE erase failed, ret=%d\r\n", ret);
        goto out;
    }

    do {
        bytes_read = webclient_read(session, buffer, CMAIW82AL_UPDATE_RX_BUF_SIZE);
        if (bytes_read < 0) {
            CLI_LOGE("IF0_UPDATE http read failed, ret=%d\r\n", bytes_read);
            goto out;
        }
        if (bytes_read == 0) {
            break;
        }
        if ((write_offset + (uint32_t)bytes_read) > partition_size) {
            CLI_LOGE("IF0_UPDATE download overflow at offset=%u read=%d\r\n", write_offset, bytes_read);
            goto out;
        }

        ret = bk_flash_partition_write(BK_PARTITION_USR_CONFIG, buffer, write_offset, bytes_read);
        if (ret != BK_OK) {
            CLI_LOGE("IF0_UPDATE write failed, offset=%u len=%d ret=%d\r\n",
                     write_offset,
                     bytes_read,
                     ret);
            goto out;
        }

        write_offset += bytes_read;
        if ((write_offset % (128 * 1024)) == 0 || write_offset == (uint32_t)content_length) {
            CLI_LOGI("IF0_UPDATE progress: %u/%d\r\n", write_offset, content_length);
        }
    } while (write_offset < (uint32_t)content_length);

    if (write_offset != (uint32_t)content_length) {
        CLI_LOGE("IF0_UPDATE download size mismatch: wrote=%u expected=%d\r\n", write_offset, content_length);
        goto out;
    }

    ret = BK_OK;

out:
    if (session) {
        webclient_close(session);
    }
    if (buffer) {
        os_free(buffer);
    }

    if (app_vfs_mount_if0() != BK_OK) {
        CLI_LOGE("IF0_UPDATE remount /if0 failed\r\n");
        if (ret == BK_OK) {
            ret = BK_FAIL;
        }
    }

    CLI_LOGI("IF0_UPDATE finish, ret=%d\r\n", ret);
    return ret;
}

static bk_err_t cli_app_update_download_ota(const char *url)
{
    int ret = BK_FAIL;

    CLI_LOGI("APP_OTA begin: %s\r\n", url);
    ret = bk_http_ota_download(url);
    CLI_LOGI("APP_OTA finish, ret=%d\r\n", ret);

    return ret == BK_OK ? BK_OK : BK_FAIL;
}

static void cli_app_update_flash_task(beken_thread_arg_t arg)
{
    cmaiw82al_update_task_arg_t *task_arg = (cmaiw82al_update_task_arg_t *)arg;
    bool is_eye_update = false;
    bool is_ota_update = false;
    bool is_if0_update = false;
    bk_err_t ret = BK_FAIL;

    if (task_arg == NULL) {
        s_update_flash_running = false;
        rtos_delete_thread(NULL);
        return;
    }

    is_eye_update = task_arg->is_eye_update;
    is_ota_update = task_arg->is_ota_update;
    is_if0_update = task_arg->is_if0_update;

    if (is_if0_update) {
        ret = cli_app_update_download_if0_image(task_arg->url);
    } else if (is_ota_update) {
        ret = cli_app_update_download_ota(task_arg->url);
    } else {
        ret = cli_app_update_download_flash_image(task_arg->url, is_eye_update);
    }

    if (is_eye_update && ret == BK_OK) {
        CLI_LOGI("EYES_UPDATE success, reboot after 500 ms\r\n");
        rtos_delay_milliseconds(500);
        bk_reboot();
    }
    if (is_if0_update && ret == BK_OK) {
        CLI_LOGI("IF0_UPDATE success, reboot after 500 ms\r\n");
        rtos_delay_milliseconds(500);
        bk_reboot();
    }

    if (task_arg->url) {
        os_free(task_arg->url);
    }
    os_free(task_arg);

    s_update_flash_running = false;
    rtos_delete_thread(NULL);
}

static void cli_app_update_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    const char *url_src = NULL;
    cmaiw82al_update_task_arg_t *task_arg = NULL;
    bool is_eye_update = false;
    bool is_ota_update = false;
    bool is_if0_update = false;
    bk_err_t ret = BK_OK;

    (void)pcWriteBuffer;
    (void)xWriteBufferLen;

    if (argc >= 2 && os_strcmp(argv[1], "flash") == 0) {
        if (argc != 3 || !cli_app_update_url_is_http(argv[2])) {
            cli_app_update_help();
            return;
        }
        url_src = argv[2];
    } else if (argc >= 2 && os_strcmp(argv[1], "eyes") == 0) {
        if (argc == 2) {
            url_src = s_default_eye_url;
        } else if (argc == 3 && cli_app_update_url_is_http(argv[2])) {
            url_src = argv[2];
        } else {
            cli_app_update_help();
            return;
        }
        is_eye_update = true;
    } else if (argc >= 2 && os_strcmp(argv[1], "ota") == 0) {
        if (argc != 3 || !cli_app_update_url_is_http(argv[2])) {
            cli_app_update_help();
            return;
        }
        url_src = argv[2];
        is_ota_update = true;
    } else if (argc >= 2 && os_strcmp(argv[1], "if0") == 0) {
        if (argc != 3 || !cli_app_update_url_is_http(argv[2])) {
            cli_app_update_help();
            return;
        }
        url_src = argv[2];
        is_if0_update = true;
    } else {
        cli_app_update_help();
        return;
    }

    if (s_update_flash_running) {
        CLI_LOGW("update flash is already running\r\n");
        return;
    }

    task_arg = os_malloc(sizeof(cmaiw82al_update_task_arg_t));
    if (task_arg == NULL) {
        CLI_LOGE("malloc update task arg failed\r\n");
        return;
    }
    os_memset(task_arg, 0, sizeof(cmaiw82al_update_task_arg_t));

    task_arg->url = os_strdup(url_src);
    if (task_arg->url == NULL) {
        CLI_LOGE("duplicate url failed\r\n");
        os_free(task_arg);
        return;
    }
    task_arg->is_eye_update = is_eye_update;
    task_arg->is_ota_update = is_ota_update;
    task_arg->is_if0_update = is_if0_update;

    s_update_flash_running = true;
    ret = rtos_create_thread(NULL,
        CMAIW82AL_UPDATE_TASK_PRIO,
        "upd_flash",
        (beken_thread_function_t)cli_app_update_flash_task,
        CMAIW82AL_UPDATE_TASK_STACK,
        (beken_thread_arg_t)task_arg);
    if (ret != BK_OK) {
        CLI_LOGE("create update flash task failed, ret=%d\r\n", ret);
        s_update_flash_running = false;
        os_free(task_arg->url);
        os_free(task_arg);
    }
}

#define APP_UPDATE_CMD_CNT (sizeof(s_app_update_commands) / sizeof(struct cli_command))

static const struct cli_command s_app_update_commands[] = {
    {"update", "update flash|eyes|ota|if0 <http-url>", cli_app_update_cmd},
};

int cli_app_update_init(void)
{
    return cli_register_commands(s_app_update_commands, APP_UPDATE_CMD_CNT);
}
