#include <driver/gpio.h>
#include <driver/gpio_types.h>
#include <components/log.h>
#include <gpio_map.h>
#include "gpio_driver.h"
#include "key_main.h"
#include "multi_button.h"
#include "system_manager.h"
#include "dialog_module.h"
#include "app_key.h"
#include "common.h"
#include "pet_scene.h"

#if CONFIG_LCD
#include "display_module.h"
#endif

#define TAG "key"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define APP_KEY_VERY_LONG_HOLD_MS 1000
#define APP_KEY_VERY_LONG_HOLD_TICKS (APP_KEY_VERY_LONG_HOLD_MS / KEY_TMR_DURATION)

static app_key_config_t key_config[] = APP_KEY_CONFIG_TABLE;
static uint32_t key_config_size = sizeof(key_config) / sizeof(app_key_config_t);

static app_key_config_t* _find_key_config_by_gpio(uint32_t gpio_id)
{
    for (uint32_t i = 0; i < key_config_size; i++) {
        if (key_config[i].gpio_id == gpio_id) {
            return &key_config[i];
        }
    }
    return NULL;
}

static void _reset_key_hold_state(app_key_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->hold_ticks = 0;
    config->very_long_sent = false;
}

static bool _trigger_pet_touch_event(app_event_t app_event)
{
    switch (app_event) {
        case APP_EVENT_TOUCH_HEAD_SHORT:
            pet_scene_handle_event(PET_EVENT_TOUCH_HEAD_SHORT);
            return true;
        case APP_EVENT_TOUCH_HEAD_DOUBLE:
            pet_scene_handle_event(PET_EVENT_TOUCH_HEAD_DOUBLE);
            return true;
        case APP_EVENT_TOUCH_HEAD_LONG:
            pet_scene_handle_event(PET_EVENT_TOUCH_HEAD_LONG);
            return true;
        case APP_EVENT_TOUCH_CHIN_SHORT:
            pet_scene_handle_event(PET_EVENT_TOUCH_CHIN_SHORT);
            return true;
        case APP_EVENT_TOUCH_CHIN_DOUBLE:
            pet_scene_handle_event(PET_EVENT_TOUCH_CHIN_DOUBLE);
            return true;
        case APP_EVENT_TOUCH_CHIN_LONG:
            pet_scene_handle_event(PET_EVENT_TOUCH_CHIN_LONG);
            return true;
        case APP_EVENT_TOUCH_CHIN_VERY_LONG:
            pet_scene_handle_event(PET_EVENT_TOUCH_CHIN_VERY_LONG);
            return true;
        default:
            return false;
    }
}

static void _trigger_key_event(app_event_t app_event)
{
    if (app_event == APP_EVENT_NONE) {
        return;
    }

    if (_trigger_pet_touch_event(app_event)) {
        return;
    }
    
    switch (app_event) {
        case APP_EVENT_VOLUME_UP:
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_SPK_UP);
            break;
        case APP_EVENT_VOLUME_DOWN:
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_SPK_DOWN);
            break;
        case APP_EVENT_SHUTDOWN:
#if CMAIW82AL_DEV_DISABLE_KEY_SHUTDOWN
            LOGW("ignore shutdown key event in development build\r\n");
            break;
#else
            dialog_module_instance()->speaker_play_prompt_tone(PROMPT_SHUTDOWN);
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_DEV_DEEP_SLEEP);
            break;
#endif
        case APP_EVENT_DIALOG:
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_DIALOG_START);
            break;
        case APP_EVENT_FACTORY_RESET:
            dialog_module_instance()->speaker_play_prompt_tone(PROMPT_FACTORY_RESET);
            system_manager_instance()->send_msg_by_event(SYSTEM_EVENT_FACTORY_RESET);
            break;
        default:
            LOGW("unknown app event: %d\r\n", app_event);
            break;
    }
}

static void _key_short_press_cb(void *arg)
{
    BUTTON_S *handle = (BUTTON_S *)arg;
    uint32_t gpio_id = 0;
    app_key_config_t *config = NULL;

    if (handle == NULL) {
        LOGE("handle is NULL\r\n");
        return;
    }

    gpio_id = (uint32_t)(handle->user_data);
    LOGI("GPIO %d short press\r\n", gpio_id);
    
    config = _find_key_config_by_gpio(gpio_id);
    if (config) {
        _reset_key_hold_state(config);
        _trigger_key_event(config->short_event);
    }
}

static void _key_long_press_cb(void *arg)
{
    BUTTON_S *handle = (BUTTON_S *)arg;
    uint32_t gpio_id = 0;
    app_key_config_t *config = NULL;

    if (handle == NULL) {
        LOGE("handle is NULL\r\n");
        return;
    }

    gpio_id = (uint32_t)(handle->user_data);
    LOGI("GPIO %d long press\r\n", gpio_id);
    
    config = _find_key_config_by_gpio(gpio_id);
    if (config) {
        _reset_key_hold_state(config);
        _trigger_key_event(config->long_event);
    }
}

static void _key_hold_press_cb(void *arg)
{
    BUTTON_S *handle = (BUTTON_S *)arg;
    uint32_t gpio_id = 0;
    app_key_config_t *config = NULL;

    if (handle == NULL) {
        LOGE("handle is NULL\r\n");
        return;
    }

    gpio_id = (uint32_t)(handle->user_data);
    config = _find_key_config_by_gpio(gpio_id);
    if ((config == NULL) || (config->very_long_event == APP_EVENT_NONE) || config->very_long_sent) {
        return;
    }

    config->hold_ticks++;
    if (config->hold_ticks >= APP_KEY_VERY_LONG_HOLD_TICKS) {
        config->very_long_sent = true;
        LOGI("GPIO %d very long press\r\n", gpio_id);
        _trigger_key_event(config->very_long_event);
    }
}

static void _key_long_press_up_cb(void *arg)
{
    BUTTON_S *handle = (BUTTON_S *)arg;
    uint32_t gpio_id = 0;
    app_key_config_t *config = NULL;

    if (handle == NULL) {
        LOGE("handle is NULL\r\n");
        return;
    }

    gpio_id = (uint32_t)(handle->user_data);
    LOGI("GPIO %d long press up\r\n", gpio_id);

    config = _find_key_config_by_gpio(gpio_id);
    if (config) {
        _reset_key_hold_state(config);
    }
}

static void _key_double_press_cb(void *arg)
{
    BUTTON_S *handle = (BUTTON_S *)arg;
    uint32_t gpio_id = 0;
    app_key_config_t *config = NULL;

    if (handle == NULL) {
        LOGE("handle is NULL\r\n");
        return;
    }

    gpio_id = (uint32_t)(handle->user_data);
    LOGI("GPIO %d double press\r\n", gpio_id);
    
    config = _find_key_config_by_gpio(gpio_id);
    if (config) {
        _reset_key_hold_state(config);
        _trigger_key_event(config->double_event);
    }
}

int key_app_get_power_gpio(void)
{
    for (uint32_t i = 0; i < key_config_size; i++) {
        if (key_config[i].gpio_id >= GPIO_NUM) {
            continue;
        }

        if ((key_config[i].short_event == APP_EVENT_SHUTDOWN) || 
            (key_config[i].double_event == APP_EVENT_SHUTDOWN) || 
            (key_config[i].long_event == APP_EVENT_SHUTDOWN) ||
            (key_config[i].very_long_event == APP_EVENT_SHUTDOWN)) {
            return key_config[i].gpio_id;
        }
    }

    return -1;
}

void key_app_register_wakeup_source(void)
{
    for (uint32_t i = 0; i < key_config_size; i++) {
        if (key_config[i].gpio_id >= GPIO_NUM) {
            LOGE("invalid GPIO: %d\r\n", key_config[i].gpio_id);
            continue;
        }

        if ((key_config[i].short_event == APP_EVENT_SHUTDOWN) || 
            (key_config[i].double_event == APP_EVENT_SHUTDOWN) || 
            (key_config[i].long_event == APP_EVENT_SHUTDOWN) ||
            (key_config[i].very_long_event == APP_EVENT_SHUTDOWN)) {
            if (key_config[i].active_level == LOW_LEVEL_TRIGGER) {
                bk_gpio_register_wakeup_source(key_config[i].gpio_id, GPIO_INT_TYPE_FALLING_EDGE);
                LOGI("register wakeup source %d falling edge\r\n", key_config[i].gpio_id);
            } else {
                bk_gpio_register_wakeup_source(key_config[i].gpio_id, GPIO_INT_TYPE_RISING_EDGE);
                LOGI("register wakeup source %d rising edge\r\n", key_config[i].gpio_id);
            }
        }
    }
}

void key_app_init(void)
{
    int ret = BK_OK;
    int gpio_level = 0;

    key_initialization();

    for (uint32_t i = 0; i < key_config_size; i++) {
        if (key_config[i].gpio_id >= GPIO_NUM) {
            LOGE("invalid GPIO: %d\r\n", key_config[i].gpio_id);
            continue;
        }

        ret = key_item_configure(
            key_config[i].gpio_id, 
            key_config[i].active_level, 
            _key_short_press_cb, 
            _key_double_press_cb, 
            _key_long_press_cb, 
            _key_hold_press_cb, 
            _key_long_press_up_cb
        );
        
        if (ret != BK_OK) {
            LOGE("configure key error, gpio: %d, ret: %d\r\n", key_config[i].gpio_id, ret);
            continue;
        }

        gpio_level = bk_gpio_get_input(key_config[i].gpio_id);
        LOGI("configure key success, gpio: %d, level: %d, active_level: %d\r\n",
             key_config[i].gpio_id, gpio_level, key_config[i].active_level);
    }
}
