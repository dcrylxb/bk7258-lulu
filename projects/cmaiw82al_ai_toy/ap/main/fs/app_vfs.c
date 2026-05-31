#include <os/os.h>
#include <components/log.h>
#include "driver/flash_partition.h"
#include "driver/gpio.h"
#include "gpio_driver.h"

#include "app_vfs.h"
#include "common.h"
#include "bk_posix.h"

#define TAG "vfs"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define FLASH_EN_GPIO                    GPIO_18
#define APP_VFS_SPI_FLASH_SIZE_15MB      0xF00000
#define APP_VFS_SPI_FLASH_SIZE_16MB      0x1000000
#define APP_VFS_SPI_FLASH_LEGACY_START   0x1000
#define APP_VFS_SPI_FLASH_LEGACY_SIZE    0x100000

static bool s_if0_mounted = false;

static void _enable_board_spi_flash(void)
{
    gpio_dev_unmap(FLASH_EN_GPIO);
    bk_gpio_enable_output(FLASH_EN_GPIO);
    bk_gpio_set_output_low(FLASH_EN_GPIO);
    rtos_delay_milliseconds(80);
    LOGI("board spi flash enabled, FLASH_EN(GPIO18)=0\r\n");
}

bk_err_t app_vfs_unmount_if0(void)
{
#if CONFIG_LITTLEFS
    int ret = umount(VFS_INTERNAL_FLASH_PATITION_0);
    if (ret != BK_OK) {
        LOGW("umount %s fail or not mounted, ret=%d\r\n", VFS_INTERNAL_FLASH_PATITION_0, ret);
        s_if0_mounted = false;
        return BK_FAIL;
    }

    s_if0_mounted = false;
    LOGI("umount %s success\r\n", VFS_INTERNAL_FLASH_PATITION_0);
    return BK_OK;
#else
    return BK_FAIL;
#endif
}

bk_err_t app_vfs_unmount_sf0(void)
{
#if CONFIG_LITTLEFS
    int ret = umount("/sf0");
    if (ret != BK_OK) {
        LOGW("umount /sf0 fail or not mounted, ret=%d\r\n", ret);
        return BK_FAIL;
    }

    LOGI("umount /sf0 success\r\n");
    return BK_OK;
#else
    return BK_FAIL;
#endif
}

#if CONFIG_FATFS && !CONFIG_LITTLEFS
static bk_err_t _mount_fatfs(void)
{
    static bool is_mounted = false;
    struct bk_fatfs_partition partition;
    char *fs_name = NULL;
    bk_err_t ret = BK_OK;

    LOGI("mount fatfs\r\n");

    if (is_mounted) {
        LOGI("fatfs already mounted\r\n");
        return BK_OK;
    }

    fs_name = "fatfs";
    partition.part_type = FATFS_DEVICE;
    partition.part_dev.device_name = FATFS_DEV_FLASH;
    partition.mount_path = VFS_INTERNAL_FLASH_PATITION_0;

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);
    if (ret != BK_OK) {
        LOGE("mount fatfs fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    is_mounted = true;

    return BK_OK;
}
#endif

#if CONFIG_LITTLEFS
static bk_err_t _mount_littlefs(void)
{
    struct bk_little_fs_partition partition;
    char *fs_name = NULL;
    bk_logic_partition_t *pt = NULL;
    bk_err_t ret = BK_OK;

    LOGI("mount littlefs\r\n");

    if (s_if0_mounted) {
        LOGI("littlefs already mounted\r\n");
        return BK_OK;
    }

    pt = bk_flash_partition_get_info(BK_PARTITION_USR_CONFIG);
    if (pt == NULL) {
        LOGE("get usr_config partition fail\r\n");
        return BK_FAIL;
    }

    fs_name = "littlefs";
    partition.part_type = LFS_FLASH;
    partition.part_flash.start_addr = pt->partition_start_addr;
    partition.part_flash.size = pt->partition_length;
    partition.mount_path = VFS_INTERNAL_FLASH_PATITION_0;

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);
    if (ret != BK_OK) {
        LOGE("mount littlefs fail, ret: %d\r\n", ret);
        return BK_FAIL;
    }

    s_if0_mounted = true;

    return BK_OK;
}

static bk_err_t _mount_spi_flash_littlefs(void)
{
    struct bk_little_fs_partition partition;
    char *fs_name = NULL;
    bk_err_t ret = BK_OK;
    const struct {
        uint32_t start_addr;
        uint32_t size;
    } candidates[] = {
        {0x0, APP_VFS_SPI_FLASH_SIZE_15MB},
        {0x0, APP_VFS_SPI_FLASH_SIZE_16MB},
        {APP_VFS_SPI_FLASH_LEGACY_START, APP_VFS_SPI_FLASH_LEGACY_SIZE},
    };

    LOGI("mount spi flash littlefs /sf0\r\n");

    fs_name = "littlefs";

    for (uint32_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        partition.part_type = LFS_SPI_FLASH;
        partition.part_flash.start_addr = candidates[i].start_addr;
        partition.part_flash.size = candidates[i].size;
        partition.mount_path = "/sf0";

        ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);
        if (ret == BK_OK) {
            LOGI("mount spi flash littlefs success start=0x%x size=0x%x\r\n",
                 candidates[i].start_addr,
                 candidates[i].size);
            return BK_OK;
        }

        LOGW("mount spi flash littlefs candidate fail start=0x%x size=0x%x ret=%d\r\n",
             candidates[i].start_addr,
             candidates[i].size,
             ret);
    }

    return BK_FAIL;
}
#endif

bk_err_t app_vfs_mount_sf0(void)
{
    _enable_board_spi_flash();

#if CONFIG_LITTLEFS
    return _mount_spi_flash_littlefs();
#else
    return BK_FAIL;
#endif
}

bk_err_t app_vfs_mount_if0(void)
{
#if CONFIG_LITTLEFS
    return _mount_littlefs();
#else
    return BK_FAIL;
#endif
}

bk_err_t app_vfs_init(void)
{
    _enable_board_spi_flash();

#if CONFIG_LITTLEFS
    app_vfs_mount_if0();
    app_vfs_mount_sf0();
#elif CONFIG_FATFS
    _mount_fatfs();
#endif

    return BK_OK;
}
