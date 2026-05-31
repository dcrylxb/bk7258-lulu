#ifndef __BK_OTA_DISPLAY_H__
#define __BK_OTA_DISPLAY_H__


#ifdef __cplusplus
extern "C" {
#endif

void bk_ota_display_init(void);

void bk_ota_display_deinit(void);

bk_err_t bk_ota_image_display_open(char *filename);

bk_err_t bk_ota_image_display_close(void);


#ifdef __cplusplus
}
#endif
#endif /* __BK_OTA_DISPLAY_H__ */
