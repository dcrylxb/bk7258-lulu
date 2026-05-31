#include <os/str.h>
#include <os/mem.h>
#include <os/os.h>
#include <components/log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <gpio_driver.h>

#include "common.h"
#include "pet_haptic.h"

#define TAG "pet_haptic"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define MS32008N1_REG_GLOBAL 0x00
#define MS32008N1_REG_DC_CTRL 0x03
#define MS32008N1_REG_CHIP_FLAG 0x0f
#define MS32008N1_CHIP_FLAG_VALUE 0x08
#define MS32008N1_GLOBAL_RUN 0x01
#define MS32008N1_GLOBAL_STANDBY_RESET 0x02
#define MS32008N1_DC_HIZ 0x00
#define MS32008N1_DC_FORWARD 0x01
#define MS32008N1_DC_REVERSE 0x02
#define MS32008N1_I2C_TIMEOUT_MS 100

typedef struct {
    const char *name;
    uint16_t default_duration_ms;
} pet_haptic_pattern_t;

static const pet_haptic_pattern_t s_haptic_patterns[] = {
    {"off", 0},
    {"tap", 120},
    {"double", 260},
    {"soft", 320},
    {"purr", 1200},
    {"alert", 600},
    {"confirm", 180},
    {"error", 450},
};

static bool s_haptic_initialized = false;
static bool s_haptic_driver_awake = false;
static char s_haptic_last_pattern[PET_HAPTIC_PATTERN_NAME_MAX] = "off";
static uint32_t s_haptic_last_duration_ms = 0;

static void pet_haptic_remember_pattern(const char *pattern, uint32_t duration_ms)
{
    os_memset(s_haptic_last_pattern, 0, sizeof(s_haptic_last_pattern));
    os_strncpy(s_haptic_last_pattern,
               pattern ? pattern : "off",
               sizeof(s_haptic_last_pattern) - 1);
    s_haptic_last_duration_ms = duration_ms;
}

static void pet_haptic_hold_driver_sleep(void)
{
    gpio_dev_unmap(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_disable_pull(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_enable_output(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_set_output_low(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    s_haptic_driver_awake = false;
}

static void pet_haptic_wake_driver(void)
{
    gpio_dev_unmap(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_disable_pull(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_enable_output(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    bk_gpio_set_output_high(ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    s_haptic_driver_awake = true;
    rtos_delay_milliseconds(50);
}

static bk_err_t pet_haptic_i2c_init(void)
{
    i2c_config_t i2c_cfg = {0};
    bk_err_t ret = BK_OK;

    i2c_cfg.baud_rate = I2C_BAUD_RATE_100KHZ;
    i2c_cfg.addr_mode = I2C_ADDR_MODE_7BIT;
    i2c_cfg.slave_addr = ALI_HAPTIC_MOTOR_I2C_ADDR;
    ret = bk_i2c_init(ALI_HAPTIC_MOTOR_I2C_ID, &i2c_cfg);
    if (ret != BK_OK && ret != BK_ERR_I2C_ID_NOT_INIT) {
        LOGW("haptic i2c init ret=%d id=%u addr=0x%02x\r\n",
             ret,
             ALI_HAPTIC_MOTOR_I2C_ID,
             ALI_HAPTIC_MOTOR_I2C_ADDR);
    }

    return ret;
}

static bk_err_t pet_haptic_ms32008n1_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};

    return bk_i2c_master_write(ALI_HAPTIC_MOTOR_I2C_ID,
                               ALI_HAPTIC_MOTOR_I2C_ADDR,
                               data,
                               sizeof(data),
                               MS32008N1_I2C_TIMEOUT_MS);
}

static bk_err_t pet_haptic_ms32008n1_read_reg(uint8_t reg, uint8_t *val)
{
    bk_err_t ret = BK_OK;

    if (val == NULL) {
        return BK_ERR_PARAM;
    }

    ret = bk_i2c_master_write(ALI_HAPTIC_MOTOR_I2C_ID,
                              ALI_HAPTIC_MOTOR_I2C_ADDR,
                              &reg,
                              1,
                              MS32008N1_I2C_TIMEOUT_MS);
    if (ret != BK_OK) {
        return ret;
    }

    return bk_i2c_master_read(ALI_HAPTIC_MOTOR_I2C_ID,
                              ALI_HAPTIC_MOTOR_I2C_ADDR,
                              val,
                              1,
                              MS32008N1_I2C_TIMEOUT_MS);
}

static void pet_haptic_ms32008n1_safe_stop(void)
{
    (void)pet_haptic_ms32008n1_write_reg(MS32008N1_REG_DC_CTRL, MS32008N1_DC_HIZ);
    (void)pet_haptic_ms32008n1_write_reg(MS32008N1_REG_GLOBAL, MS32008N1_GLOBAL_STANDBY_RESET);
    pet_haptic_hold_driver_sleep();
}

static uint32_t pet_haptic_clip_duration(const char *pattern, uint32_t duration_ms)
{
    if (duration_ms > PET_HAPTIC_MAX_DURATION_MS) {
        return PET_HAPTIC_MAX_DURATION_MS;
    }

    if (duration_ms != 0 || pattern == NULL) {
        return duration_ms;
    }

    for (uint32_t i = 0; i < sizeof(s_haptic_patterns) / sizeof(s_haptic_patterns[0]); i++) {
        if (os_strcasecmp(pattern, s_haptic_patterns[i].name) == 0) {
            return s_haptic_patterns[i].default_duration_ms;
        }
    }

    return 0;
}

bk_err_t pet_haptic_init(void)
{
    if (s_haptic_initialized) {
        return BK_OK;
    }

    pet_haptic_hold_driver_sleep();
    s_haptic_initialized = true;
    LOGI("init driver=MS32008N1_OUT5 connector=%s out=%s/%s i2c=%d addr=0x%02x nsleep=%d hw_output=%d\r\n",
         ALI_HAPTIC_MOTOR_CONNECTOR,
         ALI_HAPTIC_MOTOR_OUT_A,
         ALI_HAPTIC_MOTOR_OUT_B,
         ALI_HAPTIC_MOTOR_I2C_ID,
         ALI_HAPTIC_MOTOR_I2C_ADDR,
         ALI_HAPTIC_MOTOR_NSLEEP_GPIO,
         PET_HAPTIC_ENABLE_MS32008N1_OUTPUT);

    if (!PET_HAPTIC_ENABLE_MS32008N1_OUTPUT) {
        LOGW("MS32008N1 OUT5 hardware output disabled pending register map\r\n");
        LOGW("MS32008N1 OUT5 nSLEEP held low connector=%s gpio=%d\r\n",
             ALI_HAPTIC_MOTOR_CONNECTOR,
             ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
    }

    return BK_OK;
}

bool pet_haptic_pattern_is_allowed(const char *pattern)
{
    if (pattern == NULL || pattern[0] == '\0') {
        return false;
    }

    for (uint32_t i = 0; i < sizeof(s_haptic_patterns) / sizeof(s_haptic_patterns[0]); i++) {
        if (os_strcasecmp(pattern, s_haptic_patterns[i].name) == 0) {
            return true;
        }
    }

    return false;
}

const char *pet_haptic_normalize_pattern(const char *pattern, const char *fallback)
{
    if (pet_haptic_pattern_is_allowed(pattern)) {
        return pattern;
    }

    if (pet_haptic_pattern_is_allowed(fallback)) {
        LOGW("unknown haptic pattern fallback pattern=%s fallback=%s\r\n",
             pattern ? pattern : "(null)",
             fallback);
        return fallback;
    }

    LOGW("unknown haptic pattern fallback pattern=%s fallback=off\r\n", pattern ? pattern : "(null)");
    return "off";
}

void pet_haptic_get_status(pet_haptic_status_t *status)
{
    if (status == NULL) {
        return;
    }

    os_memset(status, 0, sizeof(*status));
    status->initialized = s_haptic_initialized;
    status->hardware_output_enabled = PET_HAPTIC_ENABLE_MS32008N1_OUTPUT ? true : false;
    status->driver_awake = s_haptic_driver_awake;
    status->nsleep_gpio = ALI_HAPTIC_MOTOR_NSLEEP_GPIO;
    status->i2c_id = ALI_HAPTIC_MOTOR_I2C_ID;
    status->i2c_addr = ALI_HAPTIC_MOTOR_I2C_ADDR;
    os_strncpy(status->last_pattern, s_haptic_last_pattern, sizeof(status->last_pattern) - 1);
    status->last_duration_ms = s_haptic_last_duration_ms;
}

bk_err_t pet_haptic_probe(pet_haptic_probe_result_t *result)
{
    bk_err_t ret = BK_OK;

    if (result == NULL) {
        return BK_ERR_PARAM;
    }

    if (!s_haptic_initialized) {
        (void)pet_haptic_init();
    }

    os_memset(result, 0, sizeof(*result));
    result->i2c_id = ALI_HAPTIC_MOTOR_I2C_ID;
    result->i2c_addr = ALI_HAPTIC_MOTOR_I2C_ADDR;

    pet_haptic_wake_driver();
    ret = pet_haptic_i2c_init();
    if (ret != BK_OK && ret != BK_ERR_I2C_ID_NOT_INIT) {
        result->ret = ret;
        pet_haptic_hold_driver_sleep();
        return ret;
    }

    ret = pet_haptic_ms32008n1_read_reg(MS32008N1_REG_CHIP_FLAG, &result->chip_flag);
    result->ret = ret;
    result->ack = (ret == BK_OK && result->chip_flag == MS32008N1_CHIP_FLAG_VALUE);

    if (ret == BK_OK) {
        (void)pet_haptic_ms32008n1_write_reg(MS32008N1_REG_GLOBAL, MS32008N1_GLOBAL_STANDBY_RESET);
    }
    pet_haptic_hold_driver_sleep();

    LOGI("haptic probe addr=0x%02x ack=%d ret=%d chip=0x%02x nsleep_gpio=%d hw_output=%d\r\n",
         result->i2c_addr,
         result->ack,
         result->ret,
         result->chip_flag,
         ALI_HAPTIC_MOTOR_NSLEEP_GPIO,
         PET_HAPTIC_ENABLE_MS32008N1_OUTPUT);

    if (ret != BK_OK) {
        return ret;
    }

    return result->ack ? BK_OK : BK_FAIL;
}

bk_err_t pet_haptic_dc_test(uint32_t duration_ms, bool reverse, pet_haptic_dc_test_result_t *result)
{
    uint32_t actual_duration_ms = duration_ms;
    bk_err_t ret = BK_OK;
    uint8_t dc_ctrl = reverse ? MS32008N1_DC_REVERSE : MS32008N1_DC_FORWARD;
    uint8_t chip_flag = 0;

    if (!s_haptic_initialized) {
        (void)pet_haptic_init();
    }

    if (actual_duration_ms == 0) {
        actual_duration_ms = PET_HAPTIC_DC_TEST_DEFAULT_MS;
    }
    if (actual_duration_ms > PET_HAPTIC_DC_TEST_MAX_MS) {
        actual_duration_ms = PET_HAPTIC_DC_TEST_MAX_MS;
    }

    if (result != NULL) {
        os_memset(result, 0, sizeof(*result));
        result->requested_duration_ms = duration_ms;
        result->actual_duration_ms = actual_duration_ms;
        result->reverse = reverse;
    }

    pet_haptic_wake_driver();
    ret = pet_haptic_i2c_init();
    if (ret != BK_OK && ret != BK_ERR_I2C_ID_NOT_INIT) {
        goto out;
    }

    ret = pet_haptic_ms32008n1_read_reg(MS32008N1_REG_CHIP_FLAG, &chip_flag);
    if (result != NULL) {
        result->chip_flag = chip_flag;
    }
    if (ret != BK_OK) {
        LOGW("dc_test read chip flag failed:%d\r\n", ret);
        goto out;
    }

    if (chip_flag != MS32008N1_CHIP_FLAG_VALUE) {
        LOGW("dc_test unexpected chip flag=0x%02x\r\n", chip_flag);
        ret = BK_FAIL;
        goto out;
    }

    ret = pet_haptic_ms32008n1_write_reg(MS32008N1_REG_GLOBAL, MS32008N1_GLOBAL_RUN);
    if (ret != BK_OK) {
        LOGW("dc_test global run failed:%d\r\n", ret);
        goto out;
    }

    ret = pet_haptic_ms32008n1_write_reg(MS32008N1_REG_DC_CTRL, dc_ctrl);
    if (ret != BK_OK) {
        LOGW("dc_test dc ctrl failed:%d\r\n", ret);
        goto out;
    }

    LOGI("haptic dc_test start duration=%u reverse=%d chip=0x%02x ctrl=0x%02x\r\n",
         actual_duration_ms,
         reverse,
         chip_flag,
         dc_ctrl);
    rtos_delay_milliseconds(actual_duration_ms);

out:
    pet_haptic_ms32008n1_safe_stop();
    if (result != NULL) {
        result->ret = ret;
    }
    LOGI("haptic dc_test done ret=%d requested=%u actual=%u reverse=%d chip=0x%02x\r\n",
         ret,
         duration_ms,
         actual_duration_ms,
         reverse,
         chip_flag);
    return ret;
}

static bk_err_t pet_haptic_ms32008n1_play_dc_pulse(uint32_t duration_ms)
{
    uint32_t clipped_duration = duration_ms;
    bk_err_t ret = BK_OK;
    uint8_t chip_flag = 0;

    if (clipped_duration == 0) {
        return BK_ERR_PARAM;
    }
    if (clipped_duration > PET_HAPTIC_BUSINESS_MAX_DURATION_MS) {
        clipped_duration = PET_HAPTIC_BUSINESS_MAX_DURATION_MS;
    }

    pet_haptic_wake_driver();
    ret = pet_haptic_i2c_init();
    if (ret != BK_OK && ret != BK_ERR_I2C_ID_NOT_INIT) {
        goto out;
    }

    ret = pet_haptic_ms32008n1_read_reg(MS32008N1_REG_CHIP_FLAG, &chip_flag);
    if (ret != BK_OK) {
        LOGW("haptic pulse read chip flag failed:%d\r\n", ret);
        goto out;
    }
    if (chip_flag != MS32008N1_CHIP_FLAG_VALUE) {
        LOGW("haptic pulse unexpected chip flag=0x%02x\r\n", chip_flag);
        ret = BK_FAIL;
        goto out;
    }

    ret = pet_haptic_ms32008n1_write_reg(MS32008N1_REG_GLOBAL, MS32008N1_GLOBAL_RUN);
    if (ret != BK_OK) {
        LOGW("haptic pulse global run failed:%d\r\n", ret);
        goto out;
    }

    ret = pet_haptic_ms32008n1_write_reg(MS32008N1_REG_DC_CTRL, MS32008N1_DC_FORWARD);
    if (ret != BK_OK) {
        LOGW("haptic pulse dc ctrl failed:%d\r\n", ret);
        goto out;
    }

    LOGI("haptic pulse start duration=%u chip=0x%02x\r\n", clipped_duration, chip_flag);
    rtos_delay_milliseconds(clipped_duration);

out:
    pet_haptic_ms32008n1_safe_stop();
    LOGI("haptic pulse done ret=%d duration=%u chip=0x%02x\r\n",
         ret,
         clipped_duration,
         chip_flag);
    return ret;
}

bk_err_t pet_haptic_stop(void)
{
    if (!s_haptic_initialized) {
        (void)pet_haptic_init();
    }

    pet_haptic_hold_driver_sleep();
    pet_haptic_remember_pattern("off", 0);
    LOGI("haptic pattern=off duration=0 connector=%s out=%s/%s\r\n",
         ALI_HAPTIC_MOTOR_CONNECTOR,
         ALI_HAPTIC_MOTOR_OUT_A,
         ALI_HAPTIC_MOTOR_OUT_B);
    return BK_OK;
}

bk_err_t pet_haptic_play(const char *pattern, uint32_t duration_ms)
{
    const char *normalized = NULL;
    uint32_t clipped_duration = 0;

    if (!s_haptic_initialized) {
        (void)pet_haptic_init();
    }

    normalized = pet_haptic_normalize_pattern(pattern, "off");
    clipped_duration = pet_haptic_clip_duration(normalized, duration_ms);

    if (os_strcasecmp(normalized, "off") == 0 || clipped_duration == 0) {
        return pet_haptic_stop();
    }

    pet_haptic_remember_pattern(normalized, clipped_duration);
    LOGI("haptic pattern=%s duration=%u driver=MS32008N1_OUT5 connector=%s out=%s/%s hw_output=%d\r\n",
         normalized,
         clipped_duration,
         ALI_HAPTIC_MOTOR_CONNECTOR,
         ALI_HAPTIC_MOTOR_OUT_A,
         ALI_HAPTIC_MOTOR_OUT_B,
         PET_HAPTIC_ENABLE_MS32008N1_OUTPUT);

    if (!PET_HAPTIC_ENABLE_MS32008N1_OUTPUT) {
        LOGW("haptic pattern accepted but MS32008N1 OUT5 hardware output disabled pending register map\r\n");
        LOGW("MS32008N1 OUT5 nSLEEP held low connector=%s gpio=%d\r\n",
             ALI_HAPTIC_MOTOR_CONNECTOR,
             ALI_HAPTIC_MOTOR_NSLEEP_GPIO);
        return BK_OK;
    }

    return pet_haptic_ms32008n1_play_dc_pulse(clipped_duration);
}
