// TODO MOVE THIS FILE to top level directory
/**
 * Copyright (C) 2023 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bme690_helper.h"
#include "esp_log.h"


/******************************************************************************/
static i2c_bus_device_handle_t intf_conf;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    i2c_bus_device_handle_t intf_info = (i2c_bus_device_handle_t)intf_ptr;
    return i2c_bus_read_bytes(intf_info, reg_addr, (uint16_t)len, reg_data);
}

/*!
 * I2C write function map to COINES platform
 */
BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    i2c_bus_device_handle_t intf_info = (i2c_bus_device_handle_t)intf_ptr;
    return i2c_bus_write_bytes(intf_info, reg_addr, len, reg_data);
}

/*!
 * Delay function map to COINES platform
 */
void bme69x_delay_us(uint32_t period, void *intf_ptr)
{
    vTaskDelay(period / 1000 / portTICK_PERIOD_MS);
}

void bme69x_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BME69X_OK:

            /* Do nothing */
            break;
        case BME69X_E_NULL_PTR:
            printf("API name [%s]  Error [%d] : Null pointer\r\n", api_name, rslt);
            break;
        case BME69X_E_COM_FAIL:
            printf("API name [%s]  Error [%d] : Communication failure\r\n", api_name, rslt);
            break;
        case BME69X_E_INVALID_LENGTH:
            printf("API name [%s]  Error [%d] : Incorrect length parameter\r\n", api_name, rslt);
            break;
        case BME69X_E_DEV_NOT_FOUND:
            printf("API name [%s]  Error [%d] : Device not found\r\n", api_name, rslt);
            break;
        case BME69X_E_SELF_TEST:
            printf("API name [%s]  Error [%d] : Self test error\r\n", api_name, rslt);
            break;
        case BME69X_W_NO_NEW_DATA:
            printf("API name [%s]  Warning [%d] : No new data found\r\n", api_name, rslt);
            break;
        default:
            printf("API name [%s]  Error [%d] : Unknown error code\r\n", api_name, rslt);
            break;
    }
}

/**
 * @brief Function to select the interface between SPI and I2C for BME69X.
 * @param[in] bme      : Structure instance of bme69x_dev
 * @param[in] intf     : Interface selection parameter (BME69X_I2C_INTF or BME69X_SPI_INTF)
 * @param[in] dev_addr : Device address (I2C address or SPI CS pin)
 * @param[in] bus_inst : I2C bus handle (for I2C)
 * @return Status of execution
 * @retval 0 -> Success
 * @retval < 0 -> Failure Info
 */
int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t dev_addr, i2c_bus_handle_t bus_inst)
{
    int8_t rslt = BME69X_OK;

    if (bme != NULL)
    {
        if (intf == BME69X_I2C_INTF)
        {
            bme->intf = BME69X_I2C_INTF;
            bme->read = bme69x_i2c_read;
            bme->write = bme69x_i2c_write;

            i2c_bus_device_handle_t i2c_device_handle = i2c_bus_device_create(bus_inst, dev_addr, 0);
            if (NULL == i2c_device_handle)
            {
                ESP_LOGE("BME69X", "i2c_bus_device_create failed");
                rslt = BME69X_E_NULL_PTR;
            }
            intf_conf = i2c_device_handle;
            bme->intf_ptr = (void *)intf_conf;
        }
        else
        {
            ESP_LOGE("BME69X", "Unknown interface type");
            rslt = BME69X_E_COM_FAIL;
        }

        bme->delay_us = bme69x_delay_us;
        bme->amb_temp = 25;
    }
    else
    {
        rslt = BME69X_E_NULL_PTR;
    }

    return rslt;
}

esp_err_t bme69x_i2c_deinit(void)
{
    ESP_LOGI("BME69X", "bme69x_i2c_deinit");
    return i2c_bus_device_delete(&intf_conf);
}
