#ifndef __DISPLAY_MODULE_H__
#define __DISPLAY_MODULE_H__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISP_ACTIVED_TEXT        "已激活"
#define DISP_ACTIVE_CODE_TEXT    "验证码"
#define DISP_UPGRADE_TEXT        "升级中"

typedef struct { 
    int8_t clk_pin;
    int8_t cs_pin;
    int8_t sda_pin;
    int8_t rst_pin;
} disp_rgb_config_t;

typedef struct {
    int8_t te_pin;
} disp_mcu_config_t;

typedef struct { 
    uint8_t spi_id;
    uint8_t dc_pin;
    uint8_t rst_pin;
    uint8_t te_pin;
} disp_spi_config_t;

typedef struct { 
    disp_spi_config_t lcd0;
    disp_spi_config_t lcd1;
} disp_dual_spi_config_t;

typedef struct {
    uint8_t qspi_id;
    uint8_t rst_pin;
    uint8_t te_pin;
} disp_qspi_config_t;

typedef struct {
    disp_qspi_config_t lcd0;
    disp_qspi_config_t lcd1;
} disp_dual_qspi_config_t;

typedef struct { 
    char *name;
    bool is_dual;
    union { 
        disp_rgb_config_t rgb;
        disp_spi_config_t spi;
        disp_mcu_config_t mcu;
        disp_dual_spi_config_t dual_spi;
        disp_qspi_config_t qspi;
        disp_dual_qspi_config_t dual_qspi;
    } config;
} disp_lcd_config_t;

typedef struct{
    super_module_t super;
    void (*dispText)(char*);
    void (*dispEmoji)(char*);
    void (*dispWifi)(bool);
    bool (*isDispReady)(void);

    bool m_is_init;
} disp_module_t;

disp_module_t *display_module_instance(void);

#ifdef __cplusplus
}
#endif

#endif // __DISPLAY_MODULE_H__