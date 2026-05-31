#ifndef __APP_VFS_H__
#define __APP_VFS_H__

#include <common/bk_err.h>

bk_err_t app_vfs_init(void);
bk_err_t app_vfs_mount_if0(void);
bk_err_t app_vfs_unmount_if0(void);
bk_err_t app_vfs_mount_sf0(void);
bk_err_t app_vfs_unmount_sf0(void);

#endif
