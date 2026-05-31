#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include <modules/pm.h>
#include <driver/pwr_clk.h>
#include <media_service.h>
#include <components/log.h>
#include <gpio_driver.h>
#include <driver/gpio.h>

#include "app_ipc.h"
#include "app_vfs.h"
#include "cli_app_global.h"
#include "boards_common.h"
#include "common.h"
#include "system_manager.h"
#include "dialog_module.h"
#include "env_module.h"
#include "pet_haptic.h"

#if CONFIG_LCD
#include "display_module.h"
#endif

#if CONFIG_BUTTON
#include "app_key.h"
#endif

#define TAG "main"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define PWR_HOLD_TIME_MS (3000)

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern int bk_cli_init(void);
extern void bk_set_jtag_mode(uint32_t cpu_id, uint32_t group_id);
extern uint32_t bk_misc_get_ap_reset_reason(void);
extern uint32_t bk_misc_get_cp_reset_reason(void);
extern void bk_delay_us(UINT32 us);

void delay_us(UINT32 us)
{
    bk_delay_us(us);
}

static void bk_enter_deepsleep(void)
{
#if CMAIW82AL_DEV_DISABLE_DEEP_SLEEP
    LOGW("deep sleep blocked in development build\r\n");
    gpio_dev_unmap(HW_LDO_GPIO);
    bk_gpio_disable_pull(HW_LDO_GPIO);
    bk_gpio_enable_output(HW_LDO_GPIO);
    bk_gpio_set_output_high(HW_LDO_GPIO);
    return;
#endif

    LOGI("enter deepsleep mode\r\n");

    key_app_register_wakeup_source();
    rtos_delay_milliseconds(100);
    bk_pm_ap_sleep_mode_set(PM_MODE_FORCE_DEEP_SLEEP);
    rtos_delay_milliseconds(10);
}

static void bk_wait_power_on(void)
{
    uint32_t press_time = 0;
    int power_gpio = -1;

    power_gpio = key_app_get_power_gpio();
    if (power_gpio < 0) {
        LOGE("get power gpio failed\r\n");
        return;
    }
    LOGI("power gpio: %d\r\n", power_gpio);

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    do {
        if (bk_gpio_get_input(power_gpio) == 0) {
            extern void delay_ms(uint32 num);
            delay_ms(500);
            press_time += 500;

            if (bk_gpio_get_input(power_gpio) != 0) {
                break;
            }
        } else {
            break;
        }
    } while (press_time < PWR_HOLD_TIME_MS);
    GLOBAL_INT_RESTORE();

    if (press_time < PWR_HOLD_TIME_MS) {
        bk_enter_deepsleep();
    }
}

void system_event_cb(system_event_e event, void *data)
{
    LOGI("event: %d\r\n", event);

    switch(event)
    {
        case SYSTEM_EVENT_DEV_ACTIVE_CODE_DISP:
            #if CONFIG_LCD
            display_module_instance()->dispText((char*)data);
            #endif
            break;

        case SYSTEM_EVENT_DEV_ACTIVE_DONE:
        case SYSTEM_EVENT_OTA_FAIL:
            #if CONFIG_LCD
            display_module_instance()->dispText(DISP_ACTIVED_TEXT);
            #endif
            break;

        case SYSTEM_EVENT_OTA_START:
            #if CONFIG_LCD
            display_module_instance()->dispText(DISP_UPGRADE_TEXT);
            #endif
            break;

        case SYSTEM_EVENT_SPK_UP:
            if (dialog_module_instance()->m_spk_volume >= 100) {
                LOGI("spk volume is max: %d\r\n", dialog_module_instance()->m_spk_volume);
                break;
            }

            if ((dialog_module_instance()->m_spk_volume + 5) >= 100){
                dialog_module_instance()->m_spk_volume = 100;
            } else {
                dialog_module_instance()->m_spk_volume += 5;
            }
            LOGI("spk volume up: %d\r\n", dialog_module_instance()->m_spk_volume);
            dialog_module_instance()->speaker_set_volume(dialog_module_instance()->m_spk_volume);
            break;
            
        case SYSTEM_EVENT_SPK_DOWN:
            if (dialog_module_instance()->m_spk_volume <= 5) {
                LOGI("spk volume is min: %d\r\n", dialog_module_instance()->m_spk_volume);
                break;
            }

            if ((dialog_module_instance()->m_spk_volume - 5) <= 5) {
                dialog_module_instance()->m_spk_volume = 5;
            } else {
                dialog_module_instance()->m_spk_volume -= 5;
            }
            LOGI("spk volume down: %d\r\n", dialog_module_instance()->m_spk_volume);
            dialog_module_instance()->speaker_set_volume(dialog_module_instance()->m_spk_volume);
            break;

        case SYSTEM_EVENT_DEV_DEEP_SLEEP:
#if CMAIW82AL_DEV_DISABLE_DEEP_SLEEP
            LOGW("deep sleep blocked in development build\r\n");
            gpio_dev_unmap(HW_LDO_GPIO);
            bk_gpio_disable_pull(HW_LDO_GPIO);
            bk_gpio_enable_output(HW_LDO_GPIO);
            bk_gpio_set_output_high(HW_LDO_GPIO);
            break;
#else
            LOGI("power off\r\n");
            rtos_delay_milliseconds(3000);
            gpio_dev_unmap(HW_LDO_GPIO);
            bk_reboot_ex(RESET_SOURCE_FORCE_DEEPSLEEP);
            break;
#endif

        case SYSTEM_EVENT_FACTORY_RESET:
            LOGI("factory reset\r\n");
            env_module_instance()->delNetInfo();
            rtos_delay_milliseconds(3000);
            bk_reboot();
            break;

        default:
            break;
    }
}

int main(void)
{
    if (bk_misc_get_ap_reset_reason() == RESET_SOURCE_FORCE_DEEPSLEEP) {
        bk_init();
        bk_enter_deepsleep();
        return 0;
    }

    bk_init();
    media_service_init();

    board_instance()->super.init();
    pet_haptic_init();

    if (bk_misc_get_cp_reset_reason() == RESET_SOURCE_DEEPPS_GPIO) {
        bk_wait_power_on();
    }

    app_ipc_init();
    app_vfs_init();

#if CONFIG_APP_CLI
    cli_app_init();
#endif

#if CONFIG_USBD_MSC
    extern void msc_storage_init(void);
    msc_storage_init();
#endif

    if (BK_OK == system_manager_instance()->super.init()) {
        system_manager_instance()->event_cb = system_event_cb;
        system_manager_instance()->super.start();
    }

#if CONFIG_BUTTON
    key_app_init();
#endif

    return 0;
}
