#ifndef __APP_KEY_H_
#define __APP_KEY_H_

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

typedef struct {
    uint8_t gpio_id;                   // GPIO 引脚
    uint8_t active_level;              // 按键激活电平
    app_event_t short_event;           // 短按业务事件
    app_event_t double_event;          // 双按业务事件
    app_event_t long_event;            // 长按业务事件
    app_event_t very_long_event;       // 超长按业务事件
    uint16_t hold_ticks;               // 长按保持计数
    bool very_long_sent;               // 超长按事件已触发
} app_key_config_t;

typedef void (*app_key_handler_t)(app_event_t event);

int key_app_get_power_gpio(void);
void key_app_register_wakeup_source(void);
void key_app_init(void);

#endif
