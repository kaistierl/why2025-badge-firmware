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

#include "badgevms_i2c_bus.h"

#include "badgevms_config.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>

#define SDA_PIN 18
#define SCL_PIN 20

#define I2C_MASTER_SCL_IO  SCL_PIN
#define I2C_MASTER_SDA_IO  SDA_PIN
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ I2C0_MASTER_FREQ_HZ
#define I2C_MASTER_TIMEOUT 100

#define TAG "badgevms_i2c_bus"

static i2c_config_t why_config = {
    .mode             = I2C_MODE_MASTER,
    .sda_io_num       = I2C_MASTER_SDA_IO,
    .sda_pullup_en    = GPIO_PULLUP_ENABLE,
    .scl_io_num       = I2C_MASTER_SCL_IO,
    .scl_pullup_en    = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
};

typedef struct {
    i2c_bus_device_t device;
    i2c_bus_handle_t handle;
    i2c_config_t     config;
    bool             devices[255];
    char const      *name;
} badgevms_i2c_bus_device_t;

typedef struct {
    i2c_device_t               device;
    badgevms_i2c_bus_device_t *bus;
    i2c_bus_device_handle_t    handle;
    uint8_t                    address;
    uint32_t                   clk_speed;
} badgevms_i2c_device_t;

i2c_device_t *badgevms_i2c_device_create(badgevms_i2c_bus_device_t *bus, uint8_t address, uint32_t clk_speed);

static int i2c_bus_open(void *dev, path_t *path, int flags, mode_t mode) {
    if (path->directory || path->filename)
        return -1;
    return 0;
}

static int i2c_bus_close(void *dev, int fd) {
    if (fd == 0)
        return 0;
    return -1;
}

static ssize_t i2c_bus_write(void *dev, int fd, void const *buf, size_t count) {
    return 0;
}

static ssize_t i2c_bus_read(void *dev, int fd, void *buf, size_t count) {
    return 0;
}

static ssize_t i2c_bus_lseek(void *dev, int fd, off_t offset, int whence) {
    return (off_t)-1;
}

static int _i2c_bus_scan(void *dev, i2c_scanresult_t *results, int num) {
    badgevms_i2c_bus_device_t *device       = dev;
    uint8_t                    device_count = i2c_bus_scan(device->handle, (uint8_t *)results, num);
    return device_count;
}

static i2c_device_t *_i2c_device_create(void *dev, uint8_t address, uint32_t clk_speed) {
    badgevms_i2c_bus_device_t *device = dev;
    if (device->devices[address]) {
        ESP_LOGW(TAG, "%s: Device %i alreadt opened", address);
        return NULL;
    }

    return badgevms_i2c_device_create(device, address, clk_speed);
}

static int i2c_device_open(void *dev, path_t *path, int flags, mode_t mode) {
    if (path->directory || path->filename)
        return -1;
    return 0;
}

static int i2c_device_close(void *dev, int fd) {
    if (fd == 0)
        return 0;
    return -1;
}

static ssize_t i2c_device_write(void *dev, int fd, void const *buf, size_t count) {
    badgevms_i2c_device_t *device = dev;
    esp_err_t              err;

    if (count == 1) {
        err = i2c_bus_write_byte(device->handle, fd, *((uint8_t *)buf));
        if (err == ESP_OK) {
            return 1;
        }

        return 0;
    }

    err = i2c_bus_write_bytes(device->handle, fd, count, (uint8_t *)buf);
    if (err == ESP_OK) {
        return count;
    }

    return 0;
}

static ssize_t i2c_device_read(void *dev, int fd, void *buf, size_t count) {
    badgevms_i2c_device_t *device = dev;
    esp_err_t              err;

    if (count == 1) {
        err = i2c_bus_read_byte(device->handle, fd, ((uint8_t *)buf));
        if (err == ESP_OK) {
            return 1;
        }

        return 0;
    }

    err = i2c_bus_read_bytes(device->handle, fd, count, (uint8_t *)buf);
    if (err == ESP_OK) {
        return count;
    }

    return 0;
}

static ssize_t i2c_device_lseek(void *dev, int fd, off_t offset, int whence) {
    return (off_t)-1;
}

static uint8_t i2c_device_get_address(void *dev) {
    badgevms_i2c_device_t *device = dev;
    return device->address;
}

static void i2c_device_destroy(void *dev) {
    badgevms_i2c_device_t *device = dev;
    task_record_resource_free(RES_DEVICE, dev);
    i2c_bus_device_delete(device->handle);
    device->bus->devices[device->address] = false;
    free(dev);
}

i2c_device_t *badgevms_i2c_device_create(badgevms_i2c_bus_device_t *bus, uint8_t address, uint32_t clk_speed) {
    ESP_LOGI(TAG, "Creating i2c device at address %u", address);

    badgevms_i2c_device_t *dev      = calloc(1, sizeof(badgevms_i2c_device_t));
    i2c_device_t          *i2c_dev  = (i2c_device_t *)dev;
    device_t              *base_dev = (device_t *)dev;

    base_dev->type     = DEVICE_TYPE_BLOCK;
    base_dev->_open    = i2c_device_open;
    base_dev->_close   = i2c_device_close;
    base_dev->_write   = i2c_device_write;
    base_dev->_read    = i2c_device_read;
    base_dev->_lseek   = i2c_device_lseek;
    base_dev->_destroy = i2c_device_destroy;

    i2c_dev->_get_address = i2c_device_get_address;

    if (!dev) {
        ESP_LOGE(TAG, "Failed to allocate i2c device");
        return NULL;
    }

    dev->bus       = bus;
    dev->clk_speed = clk_speed;
    dev->address   = address;
    dev->handle    = i2c_bus_device_create(bus->handle, address, clk_speed);
    if (!dev->handle) {
        ESP_LOGE(TAG, "Failed to allocate i2c device");
        free(dev);
        return NULL;
    }

    task_record_resource_alloc(RES_DEVICE, dev);
    dev->bus->devices[address] = true;
    return i2c_dev;
}

device_t *badgevms_i2c_bus_create(char const *name, uint8_t port, uint32_t clk_speed) {
    ESP_LOGI(TAG, "Initializing");
    badgevms_i2c_bus_device_t *dev      = calloc(1, sizeof(badgevms_i2c_bus_device_t));
    i2c_bus_device_t          *i2c_bus  = (i2c_bus_device_t *)dev;
    device_t                  *base_dev = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_BUS;
    base_dev->_open  = i2c_bus_open;
    base_dev->_close = i2c_bus_close;
    base_dev->_write = i2c_bus_write;
    base_dev->_read  = i2c_bus_read;
    base_dev->_lseek = i2c_bus_lseek;

    i2c_bus->_scan          = _i2c_bus_scan;
    i2c_bus->_device_create = _i2c_device_create;

    dev->config                  = why_config;
    dev->config.master.clk_speed = clk_speed;
    dev->name                    = strdup(name);

    i2c_port_t p = I2C_NUM_0;
    switch (port) {
        case 0: p = I2C_NUM_0; break;
        case 1: p = I2C_NUM_1; break;
        default:
    }

    dev->handle = i2c_bus_create(p, &dev->config);

    if (!dev->handle) {
        ESP_LOGE(TAG, "Failed to initialize bus");
        free(dev);
        return NULL;
    }

    return (device_t *)dev;
}
