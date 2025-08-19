/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "bme690.h"
#include "driver/i2c.h"

// Settings

#define TEST_MEMORY_LEAK_THRESHOLD (-800) // TODO investigate why it's a bit higher than 400


#define I2C_MASTER_SCL_IO       CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO       CONFIG_I2C_MASTER_SDA

#define I2C_MASTER_NUM          I2C_NUM_0               /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ      100*1000                /*!< I2C master clock frequency */

#ifdef CONFIG_BME69X_SDO_PULLED_DOWN
#define BME69X_I2C_ADDR        BME69X_I2C_ADDR_LOW
#else
#define BME69X_I2C_ADDR        BME69X_I2C_ADDR_HIGH
#endif

static bme69x_handle_t bme69x_handle = NULL;
static i2c_bus_handle_t i2c_bus;

/**
 * @brief i2c master initialization
 */
static void i2c_sensor_bme69x_init(void)
{
    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);

    TEST_ASSERT_NOT_NULL_MESSAGE(i2c_bus, "i2c_bus create returned NULL");

    bme69x_i2c_config_t i2c_bme69x_conf = {
        .i2c_handle = i2c_bus,
        .i2c_addr = BME69X_I2C_ADDR,
    };
    bme69x_sensor_create(&i2c_bme69x_conf, &bme69x_handle);
    TEST_ASSERT_NOT_NULL_MESSAGE(bme69x_handle, "BME69X create returned NULL \n");
}

int8_t run_self_test(bme69x_handle_t bme)
{
    int8_t rslt = bme69x_selftest_check(bme);

    if (rslt != BME69X_OK) {
        ESP_LOGE("BME69X", "Self-test failed with error code: %d", rslt);
    } else {
        ESP_LOGI("BME69X", "Self-test passed successfully");
    }
    return rslt;
}

TEST_CASE("BME69X self_test", "[BME69X][self_test]")
{
    printf("START: TEST_CASE BME69X self_test\n");

    esp_err_t ret = ESP_OK;

    printf("START: i2c_sensor_bme69x_init\n");
    i2c_sensor_bme69x_init();
    printf("DONE: i2c_sensor_bme69x_init\n");

    printf("START: bme69x_selftest_check\n");
    int8_t rslt = bme69x_selftest_check(bme69x_handle);
    printf("DONE: bme69x_selftest_check\n");

    if (rslt != BME69X_OK) {
        ESP_LOGE("BME69X", "Self-test failed with error code: %d", rslt);
    } else {
        ESP_LOGI("BME69X", "Self-test passed successfully");
    }
    bme69x_check_rslt("after_run_self_test", rslt);
    TEST_ASSERT_EQUAL(BME69X_OK, rslt);

    bme69x_sensor_del(bme69x_handle);
    ret = bme69x_i2c_deinit();
    i2c_bus_delete(i2c_bus);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    printf("DONE: TEST_CASE BME69X self_test\n");

}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    printf("START: check_leak\n");
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
    printf("DONE: check_leak\n");
}

void setUp(void)
{
    printf("START: setUp\n");
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    printf("DONE: setUp\n");
}

void tearDown(void)
{
    printf("START: tearDown\n");
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
    printf("DONE: tearDown\n");
}

/* Macro for count of samples to be displayed */
#define SAMPLE_COUNT  UINT16_C(10)  // Reduced for testing

TEST_CASE("BME69X forced_mode", "[BME69X][forced_mode]")
{
    printf("START: TEST_CASE BME69X forced_mode\n");

    esp_err_t ret = ESP_OK;
    int8_t rslt;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data;
    uint32_t del_period;
    uint32_t time_ms = 0;
    uint8_t n_fields;
    uint16_t sample_count = 1;

    printf("START: i2c_sensor_bme69x_init\n");
    i2c_sensor_bme69x_init();
    printf("DONE: i2c_sensor_bme69x_init\n");

    /* Configure sensor settings */
    conf.filter = BME69X_FILTER_OFF;
    conf.odr = BME69X_ODR_NONE;
    conf.os_hum = BME69X_OS_16X;
    conf.os_pres = BME69X_OS_16X;
    conf.os_temp = BME69X_OS_16X;
    rslt = bme69x_set_conf(&conf, bme69x_handle);
    bme69x_check_rslt("bme69x_set_conf", rslt);
    TEST_ASSERT_EQUAL(BME69X_OK, rslt);

    /* Configure heater settings */
    heatr_conf.enable = BME69X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, bme69x_handle);
    bme69x_check_rslt("bme69x_set_heatr_conf", rslt);
    TEST_ASSERT_EQUAL(BME69X_OK, rslt);

    printf("Sample, TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%%), Gas resistance(ohm), Status\n");

    while (sample_count <= SAMPLE_COUNT)
    {
        rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, bme69x_handle);
        bme69x_check_rslt("bme69x_set_op_mode", rslt);
        TEST_ASSERT_EQUAL(BME69X_OK, rslt);

        /* Calculate delay period in microseconds */
        del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, &conf, bme69x_handle) + (heatr_conf.heatr_dur * 1000);
        bme69x_handle->delay_us(del_period, bme69x_handle->intf_ptr);

        time_ms = esp_timer_get_time() / 1000;  // Convert to milliseconds

        /* Get sensor data */
        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, bme69x_handle);
        bme69x_check_rslt("bme69x_get_data", rslt);
        TEST_ASSERT_EQUAL(BME69X_OK, rslt);

        if (n_fields)
        {
#ifdef BME69X_USE_FPU
            printf("%u, %lu, %.2f, %.2f, %.2f, %.2f, 0x%x\n",
                   sample_count,
                   (long unsigned int)time_ms,
                   data.temperature,
                   data.pressure,
                   data.humidity,
                   data.gas_resistance,
                   data.status);
#else
            printf("%u, %lu, %d, %ld, %ld, %ld, 0x%x\n",
                   sample_count,
                   (long unsigned int)time_ms,
                   data.temperature,
                   data.pressure,
                   data.humidity,
                   data.gas_resistance,
                   data.status);
#endif
            sample_count++;
        }
    }

    bme69x_sensor_del(bme69x_handle);
    ret = bme69x_i2c_deinit();
    i2c_bus_delete(i2c_bus);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    printf("DONE: TEST_CASE BME69X forced_mode\n");
}

void app_main(void)
{
    printf("BME69X TEST \n");
    printf("BME69X_I2C_ADDR is defined with value: 0x%X\n", BME69X_I2C_ADDR);
    printf("I2C_MASTER_SDA_IO is defined with value: %d\n", I2C_MASTER_SDA_IO);
    printf("I2C_MASTER_SCL_IO is defined with value: %d\n", I2C_MASTER_SCL_IO);
    printf("GPIO_PULLUP_ENABLE is defined with value: %d\n", GPIO_PULLUP_ENABLE);

    unity_run_menu();
}
