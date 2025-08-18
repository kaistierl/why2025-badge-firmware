/* This file is part of BadgeVMS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bosch_bme690.h"
#include "badgevms_config.h"
#include "bme690.h"
#include <stdatomic.h>

#define BME690_I2C_ADDR 0x76

#define UE_SW_I2C 0

#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 20

#define I2C_MASTER_NUM     I2C_NUM_0           /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ I2C0_MASTER_FREQ_HZ /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT 100   

#define TAG "BME690"

static i2c_bus_handle_t i2c_bus;

typedef struct {
    gas_device_t        device;
    bme69x_handle_t     sensor;
    atomic_flag         open;
    struct bme69x_data  environment;
} bosch_bme690_device_t;

void why_bme69x_error_codes_print_result(int8_t rslt) {
    switch (rslt) {
        case BME69X_OK:                 break;
        case BME69X_E_NULL_PTR:         ESP_LOGE(TAG, "Error [%d] : Null pointer\r\n", rslt); break;
        case BME69X_E_COM_FAIL:         ESP_LOGE(TAG, "Error [%d] : Communication failure\r\n", rslt); break;
        case BME69X_E_INVALID_LENGTH:   ESP_LOGE(TAG, "Error [%d] : Incorrect length parameter\r\n", rslt); break;
        case BME69X_E_DEV_NOT_FOUND:    ESP_LOGE(TAG, "Error [%d] : Device not found\r\n", rslt); break;
        case BME69X_E_SELF_TEST:        ESP_LOGE(TAG, "Error [%d] : Self test error\r\n", rslt); break;
        case BME69X_W_NO_NEW_DATA:      ESP_LOGW(TAG, "Warning [%d] : No new data found\r\n", rslt); break;
        default:                        ESP_LOGE(TAG, "Error [%d] : Unknown error code", rslt); break;
    }
}

static int bme690_open(void *dev, path_t *path, int flags, mode_t mode) {
    return 0;
}

static int bme690_close(void *dev, int fd) {
    if (fd)
        return -1;
    return 0;
}

static ssize_t bme690_read(void *dev, int fd, void *buf, size_t count) {
    if (fd)
        return -1;
    return 0;
}

static ssize_t bme690_write(void *dev, int fd, void const *buf, size_t count) {
    return -1;
}

static ssize_t bme690_lseek(void *dev, int fd, off_t offset, int whence) {
    return -1;
}

static struct bme69x_data get_environment(void *dev) {
    int8_t rslt;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data;
    uint32_t del_period;
    uint8_t n_fields;

    bosch_bme690_device_t *device = dev;

    if (atomic_flag_test_and_set(&device->open)) {
        return data;
    }

    conf.filter = BME69X_FILTER_OFF;
    conf.odr = BME69X_ODR_NONE;
    conf.os_hum = BME69X_OS_16X;
    conf.os_pres = BME69X_OS_16X;
    conf.os_temp = BME69X_OS_16X;
    rslt = bme69x_set_conf(&conf, device->sensor);
    why_bme69x_error_codes_print_result(rslt);
    if (rslt != BME69X_OK) {
        return device->environment;
    }

    heatr_conf.enable = BME69X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, device->sensor);
    why_bme69x_error_codes_print_result(rslt);
    if (rslt != BME69X_OK) {
        return device->environment;
    }

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, device->sensor);
    why_bme69x_error_codes_print_result(rslt);
    if (rslt != BME69X_OK) {
        return device->environment;
    }

    del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, &conf, device->sensor) + (heatr_conf.heatr_dur * 1000);
    device->sensor->delay_us(del_period, device->sensor->intf_ptr);

    rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, device->sensor);
    why_bme69x_error_codes_print_result(rslt);
    if (rslt != BME69X_OK) {
        return device->environment;
    }

    device->environment = data;

    atomic_flag_clear(&device->open);

    return device->environment;
}

float get_pressure(void *dev) {
    struct bme69x_data data = get_environment(dev);
    
    return data.pressure;
}

float get_temperature(void *dev) {
    struct bme69x_data data = get_environment(dev);
    
    return data.temperature;
}

float get_humidity(void *dev) {
    struct bme69x_data data = get_environment(dev);
    
    return data.humidity;
}

float get_gas_resistance(void *dev) {
    struct bme69x_data data = get_environment(dev);
    
    return data.gas_resistance;
}

device_t *bosch_bme690_sensor_create() {
    bosch_bme690_device_t   *dev        = calloc(1, sizeof(bosch_bme690_device_t));
    gas_device_t            *gas_dev    = (gas_device_t *)dev;
    device_t                *base_dev   = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_GAS;
    base_dev->_open  = bme690_open;
    base_dev->_close = bme690_close;
    base_dev->_write = bme690_write;
    base_dev->_read  = bme690_read;
    base_dev->_lseek = bme690_lseek;

    gas_dev->_get_pressure = get_pressure;
    gas_dev->_get_temperature = get_temperature;
    gas_dev->_get_gas_resistance = get_gas_resistance;
    gas_dev->_get_humidity = get_humidity;

    i2c_config_t const i2c_bus_conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_MASTER_SDA_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = I2C_MASTER_SCL_IO,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    #if UE_SW_I2C
        i2c_bus = i2c_bus_create(I2C_NUM_SW_1, &i2c_bus_conf);
    #else
        i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
    #endif
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed initialising i2c bus");
        free(dev);

        return NULL;
    }

    bme69x_i2c_config_t i2c_bme690_conf = {
        .i2c_handle = i2c_bus,
        .i2c_addr   = BME690_I2C_ADDR,
    };
    bme69x_sensor_create(&i2c_bme690_conf, &dev->sensor);
    if (dev->sensor == NULL) {
        ESP_LOGE(TAG, "Failed initialising bosch bme690 sensor");
        free(dev);

        return NULL;
    }

    ESP_LOGI(TAG, "BME690 initialized");

    return (device_t *)dev;
}
