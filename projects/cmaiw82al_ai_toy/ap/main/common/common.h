#ifndef __COMMON_H__
#define __COMMON_H__

#include <common/bk_include.h>
#include <driver/i2c_types.h>

typedef enum {
    APP_EVENT_NONE = 0,
    APP_EVENT_VOLUME_UP,
    APP_EVENT_VOLUME_DOWN,
    APP_EVENT_SHUTDOWN,
    APP_EVENT_DIALOG,
    APP_EVENT_FACTORY_RESET,
    APP_EVENT_TOUCH_HEAD_SHORT,
    APP_EVENT_TOUCH_HEAD_DOUBLE,
    APP_EVENT_TOUCH_HEAD_LONG,
    APP_EVENT_TOUCH_CHIN_SHORT,
    APP_EVENT_TOUCH_CHIN_DOUBLE,
    APP_EVENT_TOUCH_CHIN_LONG,
    APP_EVENT_TOUCH_CHIN_VERY_LONG,
    APP_EVENT_BLE_PAIR,
    APP_EVENT_MAX,
} app_event_t;

#define POWER_LOCK_GPIO         GPIO_19
#define HW_LDO_GPIO             POWER_LOCK_GPIO
#define SPEAKER_PA_GPIO         GPIO_8

#define CMAIW82AL_DEV_DISABLE_KEY_SHUTDOWN 1
#define CMAIW82AL_DEV_DISABLE_DEEP_SLEEP 1

#define EYE_AVI_INTERNAL_ROOT   "/if0"
#define EYE_AVI_SPI_FLASH_ROOT  "/sf0"
#define EYE_AVI_SUBDIR          "eyes"
#define EYE_AVI_DEFAULT_NAME    "neutral.avi"
#define EYE_AVI_FILE            "/sf0/neutral.avi"
#define EYE_AVI_IDLE_NAME       "neutral.avi"
#define EYE_AVI_LISTEN_NAME     "curious.avi"
#define EYE_AVI_THINK_NAME      "pensive.avi"
#define EYE_AVI_SPEAK_NAME      "smiling.avi"
#define EYE_AVI_HAPPY_NAME      "happy.avi"
#define EYE_AVI_CAMERA_NAME     "photo.avi"
#define EYE_AVI_ERROR_NAME      "confused.avi"
#define EYE_AVI_SLEEP_NAME      "tired.avi"
#define EYE_AVI_AFRAID_NAME     "afraid.avi"
#define EYE_AVI_ANGRY_NAME      "angry.avi"
#define EYE_AVI_BORED_NAME      "bored.avi"
#define EYE_AVI_CARING_NAME     "caring.avi"
#define EYE_AVI_DOUBTFUL_NAME   "doubtful.avi"
#define EYE_AVI_FROWNING_NAME   "frowning.avi"
#define EYE_AVI_GRIMACING_NAME  "grimacing.avi"
#define EYE_AVI_SAD_NAME        "sad.avi"
#define EYE_AVI_SURPRISED_NAME  "surprised.avi"
#define EYE_AVI_WINKING_NAME    "winking.avi"
#define EYE_AVI_BOOT_NAME       EYE_AVI_HAPPY_NAME

#define ALI_OPTICAL_EN_GPIO     GPIO_40
#define ALI_OPTICAL_IN_GPIO     GPIO_55
#define ALI_MOTOR_NSLEEP_GPIO   GPIO_50
#define ALI_MOTOR_I2C_ID        I2C_ID_0
#define ALI_MOTOR_I2C_ADDR      0x10
#define ALI_HAPTIC_MOTOR_CONNECTOR "CN3"
#define ALI_HAPTIC_MOTOR_OUT_A "OUT5A"
#define ALI_HAPTIC_MOTOR_OUT_B "OUT5B"
#define ALI_HAPTIC_MOTOR_NSLEEP_GPIO ALI_MOTOR_NSLEEP_GPIO
#define ALI_HAPTIC_MOTOR_I2C_ID      ALI_MOTOR_I2C_ID
#define ALI_HAPTIC_MOTOR_I2C_ADDR    ALI_MOTOR_I2C_ADDR
#define PET_MOTION_SENSOR_BMA253_HXY 1
#define ALI_IMU_SCL_GPIO          GPIO_42
#define ALI_IMU_SDA_GPIO          GPIO_43
#define ALI_IMU_INT1_GPIO         GPIO_41
#define ALI_IMU_INT2_GPIO         GPIO_52
#define ALI_IMU_SDO_GPIO          GPIO_51
#define ALI_IMU_ADDR_SDO_LOW      0x18
#define ALI_IMU_ADDR_SDO_HIGH     0x19
#define ALI_IMU_BOSCH_CHIP_ID     0xfa
#define ALI_IMU_HXY_WHO_AM_I      0x11

// LCD
#define LCD_BACKLIGHT_GPIO      GPIO_25
#define LCD_WIDTH               160
#define LCD_HEIGHT              (2 * 160) // two 160x160 panels stacked for LVGL
#define LCD2_RESET_GPIO         GPIO_6
#define LCD1_RESET_GPIO         GPIO_53
#define CMAIW82AL_LCD_DEVICE_NAME "cmaiw82al_gc9d01_160"
/**
 * @brief Define the configuration of the LCD display.
 * @note see `disp_lcd_config_t`
 */
#define APP_LCD_CONFIG()                            \
{                                                   \
    .name = CMAIW82AL_LCD_DEVICE_NAME,              \
    .is_dual = true,                                \
    .config = {                                     \
        .dual_spi = {                               \
            .lcd0 = {                               \
                .spi_id = 0,                        \
                .dc_pin = GPIO_7,                   \
                .rst_pin = LCD2_RESET_GPIO,         \
            },                                      \
            .lcd1 = {                               \
                .spi_id = 1,                        \
                .dc_pin = GPIO_5,                   \
                .rst_pin = LCD1_RESET_GPIO,         \
            }                                       \
        }                                           \
    }                                               \
}

/**
 * @brief Define the key table.
 * @note see `app_key_config_t`
 */
#define APP_KEY_CONFIG_TABLE                        \
{                                                   \
    {                                               \
        .gpio_id = GPIO_12,                         \
        .active_level = LOW_LEVEL_TRIGGER,           \
        .short_event = APP_EVENT_DIALOG,            \
    },                                              \
    {                                               \
        .gpio_id = GPIO_46,                         \
        .active_level = LOW_LEVEL_TRIGGER,           \
        .short_event = APP_EVENT_TOUCH_HEAD_SHORT,   \
        .double_event = APP_EVENT_TOUCH_HEAD_DOUBLE, \
        .long_event = APP_EVENT_TOUCH_HEAD_LONG,     \
    },                                              \
    {                                               \
        .gpio_id = GPIO_44,                         \
        .active_level = LOW_LEVEL_TRIGGER,           \
        .short_event = APP_EVENT_TOUCH_CHIN_SHORT,   \
        .double_event = APP_EVENT_TOUCH_CHIN_DOUBLE, \
        .long_event = APP_EVENT_TOUCH_CHIN_LONG,     \
        .very_long_event = APP_EVENT_TOUCH_CHIN_VERY_LONG, \
    },                                              \
    {                                               \
        .gpio_id = GPIO_54,                         \
        .active_level = LOW_LEVEL_TRIGGER,           \
        .double_event = APP_EVENT_BLE_PAIR,          \
        .long_event = APP_EVENT_SHUTDOWN,           \
    },                                              \
}

#define USER_AGENT_NAME         "cmaiw82al-ai-toy"
#define USER_AGENT_VER          "1.0.8"

#define XIAOZHI_BACKEND_HTTP_HOST "106.55.173.79:8989"
#define XIAOZHI_OTA_VERSION_URL "http://" XIAOZHI_BACKEND_HTTP_HOST "/xiaozhi/ota/"
#define XIAOZHI_WS_SERVER_URL "ws://" XIAOZHI_BACKEND_HTTP_HOST "/xiaozhi/v1/"
#define XIAOZHI_MQTT_URI "tcp://106.55.173.79:2883"
#define XIAOZHI_UDP_HOST "106.55.173.79"
#define XIAOZHI_UDP_PORT 8888

#define DEV_WAKE_NAME            "小智"
#define DEV_WAKE_WORD            "hello"
#define DEV_BYE_WORD             "byebye"
#define DEV_BYE_WORD_ENABLED     0

#define SPEAKER_GAIN             0x24

#if CONFIG_IOT_DEV_CAMERA
#define CAMERA_I2C_ID           1         // I2C_ID_1
#define CAMERA_PWR_GPIO         GPIO_9
#define CAMERA_RST_GPIO         GPIO_28
#endif

#define DEFAULT_SPK_VOL         85
#define MIC_SAMPLE_RATE         16000
#define MIC_FRAME_DURATION      20
#define SPK_SAMPLE_RATE         16000
#define SPK_FRAME_DURATION      60

typedef struct{
    s32 (*init)(void);
    s32 (*start)(void);
    s32 (*stop)(void);
    s32 (*deinit)(void);
} super_module_t;

typedef enum{
    TASK_STATE_INIT,
    TASK_STATE_RUNNING,
    TASK_STATE_STOP
} task_state_e;

#endif
