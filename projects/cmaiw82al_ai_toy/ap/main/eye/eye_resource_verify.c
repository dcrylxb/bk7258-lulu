#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <components/log.h>
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>

#include "bk_posix.h"
#include "common.h"
#include "eye_resource_verify.h"

#define TAG "eye_res"

#define EYE_RESOURCE_VERIFY_MANIFEST_PATH "/sf0/avi_manifest.txt"
#define EYE_RESOURCE_VERIFY_MANIFEST_MAX_SIZE 4096
#define EYE_RESOURCE_VERIFY_READ_CHUNK_SIZE 4096
#define EYE_RESOURCE_VERIFY_MAX_ENTRIES 32
#define EYE_RESOURCE_VERIFY_READ_RETRY_MAX 2
#define EYE_RESOURCE_VERIFY_HASH_ATTEMPTS 4

static uint32_t eye_resource_verify_hash_update(uint32_t hash, const uint8_t *data, uint32_t len)
{
    if (data == NULL) {
        return hash;
    }

    for (uint32_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

static bk_err_t eye_resource_verify_read_full_fd(int fd, uint8_t *buff, uint32_t btr, uint32_t *br)
{
    uint32_t total_read = 0;
    uint32_t retry_count = 0;

    if (buff == NULL || br == NULL) {
        return BK_ERR_PARAM;
    }

    while (total_read < btr) {
        ssize_t read_len = read(fd, buff + total_read, btr - total_read);
        if (read_len < 0) {
            if (retry_count < EYE_RESOURCE_VERIFY_READ_RETRY_MAX) {
                retry_count++;
                rtos_delay_milliseconds(1);
                continue;
            }
            *br = total_read;
            return BK_FAIL;
        }
        if (read_len == 0) {
            break;
        }

        retry_count = 0;
        total_read += read_len;
    }

    *br = total_read;
    return total_read == btr ? BK_OK : BK_FAIL;
}

static bk_err_t eye_resource_verify_read_manifest(char **out_manifest)
{
    struct stat st = {0};
    char *manifest = NULL;
    uint32_t bytes_read = 0;
    int fd = -1;

    if (out_manifest == NULL) {
        return BK_ERR_PARAM;
    }
    *out_manifest = NULL;

    if (stat(EYE_RESOURCE_VERIFY_MANIFEST_PATH, &st) != 0 || st.st_size <= 0 ||
        (uint32_t)st.st_size > EYE_RESOURCE_VERIFY_MANIFEST_MAX_SIZE) {
        BK_LOGE(TAG, "EYES_VERIFY manifest stat failed path=%s size=%ld\r\n",
                EYE_RESOURCE_VERIFY_MANIFEST_PATH,
                (long)st.st_size);
        return BK_FAIL;
    }

    manifest = os_malloc((uint32_t)st.st_size + 1);
    if (manifest == NULL) {
        BK_LOGE(TAG, "EYES_VERIFY manifest malloc failed size=%ld\r\n", (long)st.st_size);
        return BK_ERR_NO_MEM;
    }

    fd = open(EYE_RESOURCE_VERIFY_MANIFEST_PATH, O_RDONLY);
    if (fd < 0) {
        BK_LOGE(TAG, "EYES_VERIFY manifest open failed path=%s fd=%d\r\n",
                EYE_RESOURCE_VERIFY_MANIFEST_PATH,
                fd);
        os_free(manifest);
        return BK_FAIL;
    }

    if (eye_resource_verify_read_full_fd(fd, (uint8_t *)manifest, (uint32_t)st.st_size, &bytes_read) != BK_OK ||
        bytes_read != (uint32_t)st.st_size) {
        BK_LOGE(TAG, "EYES_VERIFY manifest read failed read=%u size=%ld\r\n",
                bytes_read,
                (long)st.st_size);
        close(fd);
        os_free(manifest);
        return BK_FAIL;
    }

    close(fd);
    manifest[st.st_size] = '\0';
    *out_manifest = manifest;
    return BK_OK;
}

static bk_err_t eye_resource_verify_file_hash(const char *path, long *actual_size, uint32_t *actual_hash)
{
    struct stat st = {0};
    uint8_t *buffer = NULL;
    uint32_t hash = 2166136261u;
    long total_read = 0;
    int fd = -1;
    bk_err_t ret = BK_FAIL;

    if (path == NULL || actual_size == NULL || actual_hash == NULL) {
        return BK_ERR_PARAM;
    }

    *actual_size = -1;
    *actual_hash = 0;

    if (stat(path, &st) != 0 || st.st_size < 0) {
        return BK_FAIL;
    }
    *actual_size = (long)st.st_size;

    buffer = os_malloc(EYE_RESOURCE_VERIFY_READ_CHUNK_SIZE);
    if (buffer == NULL) {
        return BK_ERR_NO_MEM;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        os_free(buffer);
        return BK_FAIL;
    }

    while (total_read < (long)st.st_size) {
        uint32_t remain = (uint32_t)((long)st.st_size - total_read);
        uint32_t read_len = remain > EYE_RESOURCE_VERIFY_READ_CHUNK_SIZE ?
            EYE_RESOURCE_VERIFY_READ_CHUNK_SIZE : remain;
        uint32_t chunk_read = 0;

        if (eye_resource_verify_read_full_fd(fd, buffer, read_len, &chunk_read) != BK_OK ||
            chunk_read != read_len) {
            BK_LOGE(TAG, "EYES_VERIFY read failed path=%s offset=%ld read=%u want=%u\r\n",
                    path,
                    total_read,
                    chunk_read,
                    read_len);
            goto out;
        }

        hash = eye_resource_verify_hash_update(hash, buffer, chunk_read);
        total_read += chunk_read;
    }

    *actual_hash = hash;
    ret = BK_OK;

out:
    close(fd);
    os_free(buffer);
    return ret;
}

static bk_err_t eye_resource_verify_file_size(const char *path, long *actual_size)
{
    struct stat st = {0};

    if (path == NULL || actual_size == NULL) {
        return BK_ERR_PARAM;
    }

    *actual_size = -1;
    if (stat(path, &st) != 0 || st.st_size < 0) {
        return BK_FAIL;
    }

    *actual_size = (long)st.st_size;
    return BK_OK;
}

int eye_resource_verify_sf0_manifest(bool verify_hash)
{
    char *manifest = NULL;
    uint32_t ok_count = 0;
    uint32_t fail_count = 0;
    uint32_t entry_count = 0;
    bk_err_t ret = BK_OK;

    BK_LOGI(TAG, "EYES_VERIFY begin manifest=/sf0/avi_manifest.txt hash=%d\r\n", verify_hash ? 1 : 0);

    ret = eye_resource_verify_read_manifest(&manifest);
    if (ret != BK_OK) {
        BK_LOGE(TAG, "EYES_VERIFY summary ok=%u fail=%u hash=%d\r\n",
                ok_count,
                fail_count + 1,
                verify_hash ? 1 : 0);
        return ret;
    }

    for (char *line = manifest; line != NULL && *line != '\0'; ) {
        char *next = os_strchr(line, '\n');
        char entry_name[64] = {0};
        char path[96] = {0};
        uint32_t expected_size = 0;
        uint32_t expected_hash = 0;
        uint32_t actual_hash = 0;
        long actual_size = -1;
        bk_err_t file_ret = BK_FAIL;
        bool file_ok = false;

        if (next != NULL) {
            *next = '\0';
            next++;
        }

        if (line[0] == '#' || line[0] == '\0' ||
            sscanf(line, "%63s %u 0x%x", entry_name, &expected_size, &expected_hash) != 3) {
            line = next;
            continue;
        }

        entry_count++;
        snprintf(path, sizeof(path), "%s/%s", EYE_AVI_SPI_FLASH_ROOT, entry_name);

        if (verify_hash) {
            for (uint32_t attempt = 0; attempt < EYE_RESOURCE_VERIFY_HASH_ATTEMPTS; attempt++) {
                file_ret = eye_resource_verify_file_hash(path, &actual_size, &actual_hash);
                if (file_ret == BK_OK &&
                    actual_size == (long)expected_size &&
                    actual_hash == expected_hash) {
                    file_ok = true;
                    break;
                }

                if (file_ret == BK_OK && actual_size == (long)expected_size) {
                    BK_LOGW(TAG,
                            "EYES_VERIFY hash mismatch path=%s attempt=%u expected_hash=0x%08x actual_hash=0x%08x\r\n",
                            path,
                            attempt,
                            expected_hash,
                            actual_hash);
                }
                rtos_delay_milliseconds(20);
            }
        } else {
            file_ret = eye_resource_verify_file_size(path, &actual_size);
            actual_hash = expected_hash;
            file_ok = (file_ret == BK_OK && actual_size == (long)expected_size);
            if (file_ok) {
                BK_LOGI(TAG,
                        "EYES_VERIFY size-only path=%s expected_size=%u actual_size=%ld\r\n",
                        path,
                        expected_size,
                        actual_size);
            }
        }

        if (file_ok) {
            ok_count++;
        } else {
            fail_count++;
        }

        BK_LOGI(TAG,
                "EYES_VERIFY file path=%s expected_size=%u actual_size=%ld expected_hash=0x%08x actual_hash=0x%08x ret=%d\r\n",
                path,
                expected_size,
                actual_size,
                expected_hash,
                actual_hash,
                file_ret);

        if (entry_count >= EYE_RESOURCE_VERIFY_MAX_ENTRIES) {
            BK_LOGW(TAG, "EYES_VERIFY entry limit reached max=%u\r\n", EYE_RESOURCE_VERIFY_MAX_ENTRIES);
            break;
        }

        line = next;
    }

    if (entry_count == 0) {
        fail_count++;
        BK_LOGE(TAG, "EYES_VERIFY manifest has no entries\r\n");
    }

    BK_LOGI(TAG, "EYES_VERIFY summary ok=%u fail=%u hash=%d\r\n",
            ok_count,
            fail_count,
            verify_hash ? 1 : 0);
    os_free(manifest);
    return fail_count == 0 ? BK_OK : BK_FAIL;
}
