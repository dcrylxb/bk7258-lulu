#include <components/log.h>
#include <driver/gpio.h>
#include <gpio_driver.h>
#include <os/os.h>
#include <os/str.h>

#include "common.h"
#include "pet_motion.h"
#include "pet_scene.h"

#define TAG "pet_motion"

#define LOGI(format, ...) BK_LOGI(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGE(format, ...) BK_LOGE(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s][%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define PET_MOTION_BOSCH_CHIP_ID_REG       0x00
#define PET_MOTION_BOSCH_ACCD_X_LSB_REG    0x02
#define PET_MOTION_BOSCH_RANGE_REG         0x0f
#define PET_MOTION_BOSCH_BW_REG            0x10
#define PET_MOTION_BOSCH_LPW_REG           0x11
#define PET_MOTION_BOSCH_HBW_REG           0x13
#define PET_MOTION_BOSCH_SOFTRESET_REG     0x14
#define PET_MOTION_BOSCH_SOFTRESET_CMD     0xb6
#define PET_MOTION_BOSCH_RANGE_2G          0x03
#define PET_MOTION_BOSCH_BW_31HZ           0x0a
#define PET_MOTION_BOSCH_NORMAL_MODE       0x00

#define PET_MOTION_HXY_WHO_AM_I_REG        0x0f
#define PET_MOTION_HXY_CTRL1_REG           0x20
#define PET_MOTION_HXY_CTRL4_REG           0x23
#define PET_MOTION_HXY_OUT_X_L_REG         0x28
#define PET_MOTION_HXY_SOFTRESET_REG       0x68
#define PET_MOTION_HXY_SOFTRESET_CMD       0xa5
#define PET_MOTION_HXY_VERSION_REG         0x70

#define PET_MOTION_I2C_DELAY_US            5
#define PET_MOTION_RESET_SETTLE_MS         5
#define PET_MOTION_INIT_SETTLE_MS          10
#define PET_MOTION_RANGE_G                 2
#define PET_MOTION_MONITOR_TASK_NAME       "pet_motion"
#define PET_MOTION_MONITOR_TASK_PRIORITY   3
#define PET_MOTION_MONITOR_TASK_SIZE       2048
#define PET_MOTION_MONITOR_PERIOD_MS       180
#define PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS 1500
#define PET_MOTION_MONITOR_ROUTE_SAFETY_COOLDOWN_MS 300
#define PET_MOTION_TILT_AXIS_THRESHOLD_MG  650

typedef enum {
    PET_MOTION_VARIANT_UNKNOWN = 0,
    PET_MOTION_VARIANT_BOSCH,
    PET_MOTION_VARIANT_HXY,
} pet_motion_variant_t;

typedef struct {
    const char *name;
    gpio_id_t scl;
    gpio_id_t sda;
} pet_motion_bus_t;

typedef struct {
    bool ready;
    bool monitor_enabled;
    pet_motion_variant_t variant;
    pet_motion_bus_t bus;
    uint8_t addr;
    pet_motion_event_t last_event;
    beken_thread_t monitor_thread;
} pet_motion_ctx_t;

static pet_motion_ctx_t s_motion = {0};

extern void bk_delay_us(UINT32 us);

static void pet_motion_i2c_delay(void)
{
    bk_delay_us(PET_MOTION_I2C_DELAY_US);
}

static void pet_motion_scl_high(gpio_id_t scl_gpio)
{
    bk_gpio_disable_output(scl_gpio);
    bk_gpio_enable_input(scl_gpio);
    bk_gpio_enable_pull(scl_gpio);
    bk_gpio_pull_up(scl_gpio);
    pet_motion_i2c_delay();
}

static void pet_motion_scl_low(gpio_id_t scl_gpio)
{
    bk_gpio_disable_input(scl_gpio);
    bk_gpio_enable_output(scl_gpio);
    bk_gpio_set_output_low(scl_gpio);
    pet_motion_i2c_delay();
}

static void pet_motion_sda_high(gpio_id_t sda_gpio)
{
    bk_gpio_disable_output(sda_gpio);
    bk_gpio_enable_input(sda_gpio);
    bk_gpio_enable_pull(sda_gpio);
    bk_gpio_pull_up(sda_gpio);
    pet_motion_i2c_delay();
}

static void pet_motion_sda_low(gpio_id_t sda_gpio)
{
    bk_gpio_disable_input(sda_gpio);
    bk_gpio_enable_output(sda_gpio);
    bk_gpio_set_output_low(sda_gpio);
    pet_motion_i2c_delay();
}

static void pet_motion_bus_init(const pet_motion_bus_t *bus)
{
    gpio_dev_unmap(bus->scl);
    gpio_dev_unmap(bus->sda);
    pet_motion_sda_high(bus->sda);
    pet_motion_scl_high(bus->scl);
}

static void pet_motion_i2c_start(const pet_motion_bus_t *bus)
{
    pet_motion_sda_high(bus->sda);
    pet_motion_scl_high(bus->scl);
    pet_motion_sda_low(bus->sda);
    pet_motion_scl_low(bus->scl);
}

static void pet_motion_i2c_stop(const pet_motion_bus_t *bus)
{
    pet_motion_sda_low(bus->sda);
    pet_motion_scl_high(bus->scl);
    pet_motion_sda_high(bus->sda);
}

static bool pet_motion_i2c_send_byte(const pet_motion_bus_t *bus, uint8_t value)
{
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        if (value & mask) {
            pet_motion_sda_high(bus->sda);
        } else {
            pet_motion_sda_low(bus->sda);
        }
        pet_motion_scl_high(bus->scl);
        pet_motion_scl_low(bus->scl);
    }

    pet_motion_sda_high(bus->sda);
    pet_motion_scl_high(bus->scl);
    bool ack = (bk_gpio_get_input(bus->sda) == 0);
    pet_motion_scl_low(bus->scl);

    return ack;
}

static uint8_t pet_motion_i2c_read_byte(const pet_motion_bus_t *bus, bool ack)
{
    uint8_t value = 0;

    pet_motion_sda_high(bus->sda);
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        pet_motion_scl_high(bus->scl);
        if (bk_gpio_get_input(bus->sda)) {
            value |= mask;
        }
        pet_motion_scl_low(bus->scl);
    }

    if (ack) {
        pet_motion_sda_low(bus->sda);
    } else {
        pet_motion_sda_high(bus->sda);
    }
    pet_motion_scl_high(bus->scl);
    pet_motion_scl_low(bus->scl);
    pet_motion_sda_high(bus->sda);

    return value;
}

static bool pet_motion_read_reg_on(const pet_motion_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *value)
{
    if (value == NULL) {
        return false;
    }

    pet_motion_i2c_start(bus);
    if (!pet_motion_i2c_send_byte(bus, addr << 1)) {
        pet_motion_i2c_stop(bus);
        return false;
    }
    if (!pet_motion_i2c_send_byte(bus, reg)) {
        pet_motion_i2c_stop(bus);
        return false;
    }

    pet_motion_i2c_start(bus);
    if (!pet_motion_i2c_send_byte(bus, (addr << 1) | 1)) {
        pet_motion_i2c_stop(bus);
        return false;
    }

    *value = pet_motion_i2c_read_byte(bus, false);
    pet_motion_i2c_stop(bus);
    return true;
}

static bool pet_motion_read_block_on(const pet_motion_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0) {
        return false;
    }

    pet_motion_i2c_start(bus);
    if (!pet_motion_i2c_send_byte(bus, addr << 1)) {
        pet_motion_i2c_stop(bus);
        return false;
    }
    if (!pet_motion_i2c_send_byte(bus, reg)) {
        pet_motion_i2c_stop(bus);
        return false;
    }

    pet_motion_i2c_start(bus);
    if (!pet_motion_i2c_send_byte(bus, (addr << 1) | 1)) {
        pet_motion_i2c_stop(bus);
        return false;
    }

    for (uint32_t i = 0; i < len; i++) {
        buf[i] = pet_motion_i2c_read_byte(bus, i + 1 < len);
    }
    pet_motion_i2c_stop(bus);

    return true;
}

static bk_err_t pet_motion_write_reg(uint8_t reg, uint8_t value)
{
    const pet_motion_bus_t *bus = &s_motion.bus;

    if (!s_motion.ready) {
        return BK_FAIL;
    }

    pet_motion_i2c_start(bus);
    if (!pet_motion_i2c_send_byte(bus, s_motion.addr << 1)) {
        pet_motion_i2c_stop(bus);
        return BK_FAIL;
    }
    if (!pet_motion_i2c_send_byte(bus, reg)) {
        pet_motion_i2c_stop(bus);
        return BK_FAIL;
    }
    if (!pet_motion_i2c_send_byte(bus, value)) {
        pet_motion_i2c_stop(bus);
        return BK_FAIL;
    }

    pet_motion_i2c_stop(bus);
    return BK_OK;
}

static bk_err_t pet_motion_read_reg(uint8_t reg, uint8_t *value)
{
    if (!s_motion.ready) {
        return BK_FAIL;
    }

    return pet_motion_read_reg_on(&s_motion.bus, s_motion.addr, reg, value) ? BK_OK : BK_FAIL;
}

static void pet_motion_prepare_input(gpio_id_t gpio)
{
    gpio_dev_unmap(gpio);
    bk_gpio_enable_input(gpio);
    bk_gpio_enable_pull(gpio);
    bk_gpio_pull_up(gpio);
}

static int16_t pet_motion_unpack_bosch_acc(uint8_t lsb, uint8_t msb)
{
    int16_t raw = ((int16_t)msb << 4) | (lsb >> 4);

    if (raw & 0x0800) {
        raw |= 0xf000;
    }

    return raw;
}

static int16_t pet_motion_unpack_hxy_acc(uint8_t lsb, uint8_t msb)
{
    return ((int16_t)((uint16_t)msb << 8 | lsb)) >> 4;
}

static const char *pet_motion_variant_name(pet_motion_variant_t variant)
{
    switch (variant) {
    case PET_MOTION_VARIANT_BOSCH:
        return "bosch";
    case PET_MOTION_VARIANT_HXY:
        return "hxy";
    default:
        return "unknown";
    }
}

static int32_t pet_motion_raw_to_mg(int16_t raw)
{
    return ((int32_t)raw * PET_MOTION_RANGE_G * 1000) / 2048;
}

static int32_t pet_motion_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static uint32_t pet_motion_magnitude_mg(int32_t x_mg, int32_t y_mg, int32_t z_mg)
{
    uint32_t ax = (uint32_t)pet_motion_abs_i32(x_mg);
    uint32_t ay = (uint32_t)pet_motion_abs_i32(y_mg);
    uint32_t az = (uint32_t)pet_motion_abs_i32(z_mg);

    return ax + ay + az;
}

static bk_err_t pet_motion_find_device(void)
{
    static const pet_motion_bus_t buses[] = {
        {"normal", ALI_IMU_SCL_GPIO, ALI_IMU_SDA_GPIO},
        {"swapped", ALI_IMU_SDA_GPIO, ALI_IMU_SCL_GPIO},
    };
    static const uint8_t addrs[] = {
        ALI_IMU_ADDR_SDO_LOW,
        ALI_IMU_ADDR_SDO_HIGH,
    };

    for (uint32_t i = 0; i < sizeof(buses) / sizeof(buses[0]); i++) {
        pet_motion_bus_init(&buses[i]);
        LOGI("%s idle SCL(GPIO%u)=%u SDA(GPIO%u)=%u\r\n",
             buses[i].name,
             buses[i].scl,
             bk_gpio_get_input(buses[i].scl) ? 1 : 0,
             buses[i].sda,
             bk_gpio_get_input(buses[i].sda) ? 1 : 0);

        if (!bk_gpio_get_input(buses[i].scl) || !bk_gpio_get_input(buses[i].sda)) {
            LOGW("%s scan skipped, bus is not idle-high\r\n", buses[i].name);
            continue;
        }

        for (uint32_t j = 0; j < sizeof(addrs) / sizeof(addrs[0]); j++) {
            uint8_t chip_id = 0;
            uint8_t hxy_who = 0;
            uint8_t hxy_version = 0;

            if (pet_motion_read_reg_on(&buses[i], addrs[j], PET_MOTION_BOSCH_CHIP_ID_REG, &chip_id)) {
                LOGI("%s addr=0x%02x bosch chip_id=0x%02x%s\r\n",
                     buses[i].name,
                     addrs[j],
                     chip_id,
                     chip_id == ALI_IMU_BOSCH_CHIP_ID ? " match" : " mismatch");
            } else {
                LOGD("%s addr=0x%02x bosch chip_id nack\r\n", buses[i].name, addrs[j]);
            }

            if (chip_id == ALI_IMU_BOSCH_CHIP_ID) {
                s_motion.bus = buses[i];
                s_motion.addr = addrs[j];
                s_motion.variant = PET_MOTION_VARIANT_BOSCH;
                return BK_OK;
            }

            if (!pet_motion_read_reg_on(&buses[i], addrs[j], PET_MOTION_HXY_WHO_AM_I_REG, &hxy_who)) {
                LOGD("%s addr=0x%02x hxy who_am_i nack\r\n", buses[i].name, addrs[j]);
                continue;
            }

            (void)pet_motion_read_reg_on(&buses[i], addrs[j], PET_MOTION_HXY_VERSION_REG, &hxy_version);
            LOGI("%s addr=0x%02x hxy who_am_i=0x%02x version=0x%02x%s\r\n",
                 buses[i].name,
                 addrs[j],
                 hxy_who,
                 hxy_version,
                 hxy_who == ALI_IMU_HXY_WHO_AM_I ? " match" : " mismatch");

            if (hxy_who == ALI_IMU_HXY_WHO_AM_I) {
                s_motion.bus = buses[i];
                s_motion.addr = addrs[j];
                s_motion.variant = PET_MOTION_VARIANT_HXY;
                return BK_OK;
            }
        }

        pet_motion_i2c_stop(&buses[i]);
    }

    return BK_FAIL;
}

static bool pet_motion_monitor_route_allowed(pet_motion_event_t event,
                                             pet_motion_event_t last_routed,
                                             uint32_t now_ms,
                                             uint32_t last_route_ms)
{
    uint32_t cooldown_ms = PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS;

    if (event == PET_MOTION_EVENT_NONE || event == PET_MOTION_EVENT_STABLE) {
        return false;
    }
    if (event == last_routed) {
        return false;
    }
    if (event == PET_MOTION_EVENT_FREEFALL || event == PET_MOTION_EVENT_IMPACT) {
        cooldown_ms = PET_MOTION_MONITOR_ROUTE_SAFETY_COOLDOWN_MS;
    }

    return (uint32_t)(now_ms - last_route_ms) >= cooldown_ms;
}

static void pet_motion_monitor_task(void *arg)
{
    pet_motion_sample_t sample = {0};
    pet_motion_event_t last_routed = PET_MOTION_EVENT_NONE;
    uint32_t last_route_ms = rtos_get_time() - PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS;

    (void)arg;
    LOGI("monitor task start period=%ums\r\n", PET_MOTION_MONITOR_PERIOD_MS);

    while (s_motion.monitor_enabled) {
        if (pet_motion_read_sample(&sample) == BK_OK) {
            pet_motion_event_t event = pet_motion_classify_sample(&sample);

            s_motion.last_event = event;
            uint32_t now_ms = rtos_get_time();
            if (pet_motion_monitor_route_allowed(event, last_routed, now_ms, last_route_ms)) {
                last_routed = event;
                last_route_ms = now_ms;
                (void)pet_motion_route_event(event);
            }
        }
        rtos_delay_milliseconds(PET_MOTION_MONITOR_PERIOD_MS);
    }

    s_motion.monitor_thread = NULL;
    rtos_delete_thread(NULL);
}

bk_err_t pet_motion_init(void)
{
    pet_motion_prepare_input(ALI_IMU_INT1_GPIO);
    pet_motion_prepare_input(ALI_IMU_INT2_GPIO);
    pet_motion_prepare_input(ALI_IMU_SDO_GPIO);
    LOGI("init sensor=BMA253/HXY scl=%d sda=%d int1=%d int2=%d sdo=%d\r\n",
         ALI_IMU_SCL_GPIO,
         ALI_IMU_SDA_GPIO,
         ALI_IMU_INT1_GPIO,
         ALI_IMU_INT2_GPIO,
         ALI_IMU_SDO_GPIO);
    return BK_OK;
}

bk_err_t pet_motion_probe(void)
{
    uint8_t id = 0;
    uint8_t cfg0 = 0;
    uint8_t cfg1 = 0;
    uint8_t cfg2 = 0;
    bk_err_t ret = BK_OK;

    if (s_motion.ready) {
        return BK_OK;
    }

    (void)pet_motion_init();
    LOGI("probe begin INT1(GPIO%u)=%u INT2(GPIO%u)=%u SDO(GPIO%u)=%u\r\n",
         ALI_IMU_INT1_GPIO,
         bk_gpio_get_input(ALI_IMU_INT1_GPIO) ? 1 : 0,
         ALI_IMU_INT2_GPIO,
         bk_gpio_get_input(ALI_IMU_INT2_GPIO) ? 1 : 0,
         ALI_IMU_SDO_GPIO,
         bk_gpio_get_input(ALI_IMU_SDO_GPIO) ? 1 : 0);

    ret = pet_motion_find_device();
    if (ret != BK_OK) {
        LOGW("BMA253/HXY not found on GPIO42/43 addr 0x18/0x19; check power, soldering, pull-ups, and SCL/SDA continuity\r\n");
        return ret;
    }

    s_motion.ready = true;
    LOGI("found BMA253 variant=%s bus=%s addr=0x%02x\r\n",
         pet_motion_variant_name(s_motion.variant),
         s_motion.bus.name,
         s_motion.addr);

    if (s_motion.variant == PET_MOTION_VARIANT_HXY) {
        (void)pet_motion_write_reg(PET_MOTION_HXY_SOFTRESET_REG, PET_MOTION_HXY_SOFTRESET_CMD);
    } else {
        (void)pet_motion_write_reg(PET_MOTION_BOSCH_SOFTRESET_REG, PET_MOTION_BOSCH_SOFTRESET_CMD);
    }
    rtos_delay_milliseconds(PET_MOTION_RESET_SETTLE_MS);

    ret = pet_motion_read_reg(s_motion.variant == PET_MOTION_VARIANT_HXY ?
                              PET_MOTION_HXY_WHO_AM_I_REG :
                              PET_MOTION_BOSCH_CHIP_ID_REG,
                              &id);
    if (ret != BK_OK ||
        (s_motion.variant == PET_MOTION_VARIANT_HXY && id != ALI_IMU_HXY_WHO_AM_I) ||
        (s_motion.variant == PET_MOTION_VARIANT_BOSCH && id != ALI_IMU_BOSCH_CHIP_ID)) {
        LOGE("chip id after reset invalid variant=%s ret=%d id=0x%02x\r\n",
             pet_motion_variant_name(s_motion.variant),
             ret,
             id);
        s_motion.ready = false;
        return BK_FAIL;
    }

    if (s_motion.variant == PET_MOTION_VARIANT_HXY) {
        ret = pet_motion_write_reg(PET_MOTION_HXY_CTRL1_REG, 0x27);
        ret |= pet_motion_write_reg(PET_MOTION_HXY_CTRL4_REG, 0x00);
    } else {
        ret = pet_motion_write_reg(PET_MOTION_BOSCH_RANGE_REG, PET_MOTION_BOSCH_RANGE_2G);
        ret |= pet_motion_write_reg(PET_MOTION_BOSCH_BW_REG, PET_MOTION_BOSCH_BW_31HZ);
        ret |= pet_motion_write_reg(PET_MOTION_BOSCH_LPW_REG, PET_MOTION_BOSCH_NORMAL_MODE);
        ret |= pet_motion_write_reg(PET_MOTION_BOSCH_HBW_REG, 0x00);
    }
    rtos_delay_milliseconds(PET_MOTION_INIT_SETTLE_MS);

    if (ret != BK_OK) {
        LOGE("configure failed ret=%d\r\n", ret);
        s_motion.ready = false;
        return ret;
    }

    if (s_motion.variant == PET_MOTION_VARIANT_HXY) {
        (void)pet_motion_read_reg(PET_MOTION_HXY_CTRL1_REG, &cfg0);
        (void)pet_motion_read_reg(PET_MOTION_HXY_CTRL4_REG, &cfg1);
        (void)pet_motion_read_reg(PET_MOTION_HXY_VERSION_REG, &cfg2);
        LOGI("configured hxy ctrl1=0x%02x ctrl4=0x%02x version=0x%02x\r\n", cfg0, cfg1, cfg2);
    } else {
        (void)pet_motion_read_reg(PET_MOTION_BOSCH_RANGE_REG, &cfg0);
        (void)pet_motion_read_reg(PET_MOTION_BOSCH_BW_REG, &cfg1);
        (void)pet_motion_read_reg(PET_MOTION_BOSCH_LPW_REG, &cfg2);
        LOGI("configured bosch range=0x%02x bw=0x%02x lpw=0x%02x\r\n", cfg0, cfg1, cfg2);
    }

    return BK_OK;
}

bk_err_t pet_motion_read_sample(pet_motion_sample_t *sample)
{
    uint8_t data[6] = {0};

    if (sample == NULL) {
        return BK_ERR_PARAM;
    }
    if (!s_motion.ready && pet_motion_probe() != BK_OK) {
        return BK_FAIL;
    }

    if (!pet_motion_read_block_on(&s_motion.bus,
                                  s_motion.addr,
                                  s_motion.variant == PET_MOTION_VARIANT_HXY ?
                                  (PET_MOTION_HXY_OUT_X_L_REG | 0x80) :
                                  PET_MOTION_BOSCH_ACCD_X_LSB_REG,
                                  data,
                                  sizeof(data))) {
        return BK_FAIL;
    }

    if (s_motion.variant == PET_MOTION_VARIANT_HXY) {
        sample->x_raw = pet_motion_unpack_hxy_acc(data[0], data[1]);
        sample->y_raw = pet_motion_unpack_hxy_acc(data[2], data[3]);
        sample->z_raw = pet_motion_unpack_hxy_acc(data[4], data[5]);
    } else {
        sample->x_raw = pet_motion_unpack_bosch_acc(data[0], data[1]);
        sample->y_raw = pet_motion_unpack_bosch_acc(data[2], data[3]);
        sample->z_raw = pet_motion_unpack_bosch_acc(data[4], data[5]);
    }

    sample->x_mg = pet_motion_raw_to_mg(sample->x_raw);
    sample->y_mg = pet_motion_raw_to_mg(sample->y_raw);
    sample->z_mg = pet_motion_raw_to_mg(sample->z_raw);
    sample->magnitude_mg = pet_motion_magnitude_mg(sample->x_mg, sample->y_mg, sample->z_mg);
    sample->timestamp_ms = rtos_get_time();
    sample->addr = s_motion.addr;
    sample->bus_name = s_motion.bus.name;
    sample->variant_name = pet_motion_variant_name(s_motion.variant);

    return BK_OK;
}

pet_motion_event_t pet_motion_classify_sample(const pet_motion_sample_t *sample)
{
    uint32_t mag = 0;
    int32_t ax = 0;
    int32_t ay = 0;
    int32_t az = 0;

    if (sample == NULL) {
        return PET_MOTION_EVENT_NONE;
    }

    mag = sample->magnitude_mg;
    ax = pet_motion_abs_i32(sample->x_mg);
    ay = pet_motion_abs_i32(sample->y_mg);
    az = pet_motion_abs_i32(sample->z_mg);

    if (mag < 300) {
        return PET_MOTION_EVENT_FREEFALL;
    }
    if (mag > 2500) {
        return PET_MOTION_EVENT_IMPACT;
    }
    if (mag > 1800) {
        return PET_MOTION_EVENT_STRONG_SHAKE;
    }
    if (ax > ay && ax > az && ax > PET_MOTION_TILT_AXIS_THRESHOLD_MG) {
        return sample->x_mg < 0 ? PET_MOTION_EVENT_TILT_LEFT : PET_MOTION_EVENT_TILT_RIGHT;
    }
    if (ay > ax && ay > az && ay > PET_MOTION_TILT_AXIS_THRESHOLD_MG) {
        return sample->y_mg < 0 ? PET_MOTION_EVENT_TILT_LEFT : PET_MOTION_EVENT_TILT_RIGHT;
    }
    if (mag > 1300 && az < 950) {
        return PET_MOTION_EVENT_GENTLE_SHAKE;
    }
    if (mag >= 700 && mag <= 1300) {
        return PET_MOTION_EVENT_STABLE;
    }

    return PET_MOTION_EVENT_NONE;
}

const char *pet_motion_event_name(pet_motion_event_t event)
{
    switch (event) {
    case PET_MOTION_EVENT_STABLE:
        return "stable";
    case PET_MOTION_EVENT_PICKED_UP:
        return "picked_up";
    case PET_MOTION_EVENT_PUT_DOWN:
        return "put_down";
    case PET_MOTION_EVENT_GENTLE_SHAKE:
        return "gentle_shake";
    case PET_MOTION_EVENT_STRONG_SHAKE:
        return "strong_shake";
    case PET_MOTION_EVENT_FREEFALL:
        return "freefall";
    case PET_MOTION_EVENT_IMPACT:
        return "impact";
    case PET_MOTION_EVENT_TILT_LEFT:
        return "tilt_left";
    case PET_MOTION_EVENT_TILT_RIGHT:
        return "tilt_right";
    default:
        return "none";
    }
}

pet_motion_event_t pet_motion_event_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return PET_MOTION_EVENT_NONE;
    }

    if (os_strcasecmp(name, "stable") == 0) {
        return PET_MOTION_EVENT_STABLE;
    }
    if (os_strcasecmp(name, "picked_up") == 0) {
        return PET_MOTION_EVENT_PICKED_UP;
    }
    if (os_strcasecmp(name, "put_down") == 0) {
        return PET_MOTION_EVENT_PUT_DOWN;
    }
    if (os_strcasecmp(name, "gentle_shake") == 0) {
        return PET_MOTION_EVENT_GENTLE_SHAKE;
    }
    if (os_strcasecmp(name, "strong_shake") == 0) {
        return PET_MOTION_EVENT_STRONG_SHAKE;
    }
    if (os_strcasecmp(name, "freefall") == 0) {
        return PET_MOTION_EVENT_FREEFALL;
    }
    if (os_strcasecmp(name, "impact") == 0) {
        return PET_MOTION_EVENT_IMPACT;
    }
    if (os_strcasecmp(name, "tilt_left") == 0) {
        return PET_MOTION_EVENT_TILT_LEFT;
    }
    if (os_strcasecmp(name, "tilt_right") == 0) {
        return PET_MOTION_EVENT_TILT_RIGHT;
    }

    return PET_MOTION_EVENT_NONE;
}

pet_event_type_t pet_motion_to_pet_event(pet_motion_event_t event)
{
    switch (event) {
    case PET_MOTION_EVENT_PICKED_UP:
        return PET_EVENT_MOTION_PICKED_UP;
    case PET_MOTION_EVENT_PUT_DOWN:
        return PET_EVENT_MOTION_PUT_DOWN;
    case PET_MOTION_EVENT_GENTLE_SHAKE:
        return PET_EVENT_MOTION_GENTLE_SHAKE;
    case PET_MOTION_EVENT_STRONG_SHAKE:
        return PET_EVENT_MOTION_STRONG_SHAKE;
    case PET_MOTION_EVENT_FREEFALL:
        return PET_EVENT_MOTION_FREEFALL;
    case PET_MOTION_EVENT_IMPACT:
        return PET_EVENT_MOTION_IMPACT;
    case PET_MOTION_EVENT_TILT_LEFT:
        return PET_EVENT_MOTION_TILT_LEFT;
    case PET_MOTION_EVENT_TILT_RIGHT:
        return PET_EVENT_MOTION_TILT_RIGHT;
    default:
        return PET_EVENT_NONE;
    }
}

bk_err_t pet_motion_route_event(pet_motion_event_t event)
{
    switch (event) {
    case PET_MOTION_EVENT_PICKED_UP:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_PICKED_UP));
        return pet_scene_handle_event(PET_EVENT_MOTION_PICKED_UP);
    case PET_MOTION_EVENT_PUT_DOWN:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_PUT_DOWN));
        return pet_scene_handle_event(PET_EVENT_MOTION_PUT_DOWN);
    case PET_MOTION_EVENT_GENTLE_SHAKE:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_GENTLE_SHAKE));
        return pet_scene_handle_event(PET_EVENT_MOTION_GENTLE_SHAKE);
    case PET_MOTION_EVENT_STRONG_SHAKE:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_STRONG_SHAKE));
        return pet_scene_handle_event(PET_EVENT_MOTION_STRONG_SHAKE);
    case PET_MOTION_EVENT_FREEFALL:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_FREEFALL));
        return pet_scene_handle_event(PET_EVENT_MOTION_FREEFALL);
    case PET_MOTION_EVENT_IMPACT:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_IMPACT));
        return pet_scene_handle_event(PET_EVENT_MOTION_IMPACT);
    case PET_MOTION_EVENT_TILT_LEFT:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_TILT_LEFT));
        return pet_scene_handle_event(PET_EVENT_MOTION_TILT_LEFT);
    case PET_MOTION_EVENT_TILT_RIGHT:
        LOGI("route motion=%s pet_event=%s\r\n",
             pet_motion_event_name(event),
             pet_brain_event_name(PET_EVENT_MOTION_TILT_RIGHT));
        return pet_scene_handle_event(PET_EVENT_MOTION_TILT_RIGHT);
    default:
        return BK_ERR_PARAM;
    }
}

bk_err_t pet_motion_set_monitor_enabled(bool enabled)
{
    bk_err_t ret = BK_OK;

    if (enabled == s_motion.monitor_enabled) {
        return BK_OK;
    }

    if (enabled && !s_motion.ready && pet_motion_probe() != BK_OK) {
        return BK_FAIL;
    }

    s_motion.monitor_enabled = enabled;
    if (!enabled) {
        LOGI("monitor disabled\r\n");
        return BK_OK;
    }

    ret = rtos_create_thread(&s_motion.monitor_thread,
                             PET_MOTION_MONITOR_TASK_PRIORITY,
                             PET_MOTION_MONITOR_TASK_NAME,
                             pet_motion_monitor_task,
                             PET_MOTION_MONITOR_TASK_SIZE,
                             NULL);
    if (ret != BK_OK) {
        s_motion.monitor_enabled = false;
        s_motion.monitor_thread = NULL;
        LOGE("create monitor task failed ret=%d\r\n", ret);
        return ret;
    }

    LOGI("monitor enabled\r\n");
    return BK_OK;
}

void pet_motion_get_status(pet_motion_status_t *status)
{
    if (status == NULL) {
        return;
    }

    status->ready = s_motion.ready;
    status->monitor_enabled = s_motion.monitor_enabled;
    status->addr = s_motion.addr;
    status->bus_name = s_motion.bus.name ? s_motion.bus.name : "none";
    status->variant_name = pet_motion_variant_name(s_motion.variant);
    status->last_event = s_motion.last_event;
}
