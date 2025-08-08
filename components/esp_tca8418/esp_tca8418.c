/*
 * SPDX-FileCopyrightText: 2024 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include "sys/param.h"

#include "badgevms_config.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "esp_idf_version.h"
#include "esp_log.h"
// #include <algorithm>
#include "esp_tca8418.h"

// TCA8418 register constants
#define REG_INTERRUPT_STATUS 0x02
#define REG_KEY_LOCK_EVT_COUNT 0x03
#define REG_KEY_EVENT_A 0x04
#define REG_GPIO_INT_STAT1 0x11
#define REG_GPIO_INT_STAT2 0x12
#define REG_GPIO_INT_STAT3 0x13
#define REG_GPIO_INT_EN1 0x1A
#define REG_GPIO_INT_EN2 0x1B
#define REG_GPIO_INT_EN3 0x1C
#define REG_KEY_PRESS_GPIO1 0x1D
#define REG_KEY_PRESS_GPIO2 0x1E
#define REG_KEY_PRESS_GPIO3 0x1F
#define REG_GPI_EM1 0x20
#define REG_GPI_EM2 0x21
#define REG_GPI_EM3 0x22
#define REG_GPIO_DIRECTION_1 0x23
#define REG_GPIO_DIRECTION_2 0x24
#define REG_GPIO_DIRECTION_3 0x25
#define REG_GPIO_INTERRUPT_LVL1 0x26
#define REG_GPIO_INTERRUPT_LVL2 0x27
#define REG_GPIO_INTERRUPT_LVL3 0x28
#define REG_DEBOUNCE_DIS1 0x29
#define REG_DEBOUNCE_DIS2 0x2A
#define REG_DEBOUNCE_DIS3 0x2B

static const char *TAG = "tca8418";

static uint8_t readRegister(tca8418_dev_t *tca8418_dev, uint8_t reg);
static void writeRegister(tca8418_dev_t *tca8418_dev, uint8_t reg, uint8_t value);

tca8418_dev_t *tca8418_create(gpio_num_t scl_pin, gpio_num_t sda_pin, uint8_t i2c_address, gpio_num_t notify_pin, uint8_t rows, uint8_t cols)
{
    tca8418_dev_t *tca8418_dev = calloc(1, sizeof(tca8418_dev_t));
    if (tca8418_dev == NULL)
    {
        ESP_LOGE(TAG, "calloc memory failed");
        return NULL;
    }

    tca8418_dev->i2c_address = i2c_address;
    tca8418_dev->scl_pin = scl_pin;
    tca8418_dev->sda_pin = sda_pin;
    tca8418_dev->notify_pin = notify_pin;
    tca8418_dev->rows = rows;
    tca8418_dev->cols = cols;

    if (tca8418_dev->i2c_address == 0)
    {
        tca8418_dev->i2c_address = TCA8418_I2C_DEFAULT_ADDRESS;
    }
    if (tca8418_dev->rows == 0)
    {
        tca8418_dev->rows = TCA8418_MAX_ROW_COUNT;
    }
    if (tca8418_dev->cols == 0)
    {
        tca8418_dev->cols = TCA8418_MAX_COLUMN_COUNT;
    }
    ESP_LOGI(TAG, "i2c_address: %02x, rows: %u, cols: %u", tca8418_dev->i2c_address, tca8418_dev->rows, tca8418_dev->cols);

    // Bit masks used for rows / columns
    const uint8_t ROW_COL_BIT_MASK[8] =
        {
            0b00000001, // one row / column
            0b00000011, // two rows / columns
            0b00000111, // three rows / columns
            0b00001111, // four rows / columns
            0b00011111, // five rows / columns
            0b00111111, // six rows / columns
            0b01111111, // seven rows / columns
            0b11111111, // eight rows / columns
        };

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = tca8418_dev->sda_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = tca8418_dev->scl_pin,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C0_MASTER_FREQ_HZ,
    };
    ESP_LOGI(TAG, "Initializing I2C bus (scl:%d, sda:%d)", tca8418_dev->scl_pin, tca8418_dev->sda_pin);
    tca8418_dev->bus_handle = i2c_bus_create(I2C_NUM_0, &conf);
    if (tca8418_dev->bus_handle == NULL)
    {
        ESP_LOGE(TAG, "failed creating i2c bus handle");
        return NULL;
    }

    ESP_LOGI(TAG, "Initializing device:%02x", tca8418_dev->i2c_address);
    tca8418_dev->dev_handle = i2c_bus_device_create(tca8418_dev->bus_handle, tca8418_dev->i2c_address, 0);
    if (tca8418_dev->dev_handle == NULL)
    {
        ESP_LOGE(TAG, "failed creating i2c device handle");
        return NULL;
    }
    ESP_LOGI(TAG, "Device %02x found! Configuring for %zu (cols), %zu (rows)", tca8418_dev->i2c_address, tca8418_dev->cols, tca8418_dev->rows);

    // set default all GPIO pins to INPUT.
    writeRegister(tca8418_dev, REG_GPIO_DIRECTION_1, 0x00);
    writeRegister(tca8418_dev, REG_GPIO_DIRECTION_2, 0x00);
    writeRegister(tca8418_dev, REG_GPIO_DIRECTION_3, 0x00);

    // add all pins to key events.
    writeRegister(tca8418_dev, REG_GPI_EM1, 0xFF);
    writeRegister(tca8418_dev, REG_GPI_EM2, 0xFF);
    writeRegister(tca8418_dev, REG_GPI_EM3, 0xFF);

    // set all pins to FALLING interrupts.
    writeRegister(tca8418_dev, REG_GPIO_INTERRUPT_LVL1, 0x00);
    writeRegister(tca8418_dev, REG_GPIO_INTERRUPT_LVL2, 0x00);
    writeRegister(tca8418_dev, REG_GPIO_INTERRUPT_LVL3, 0x00);

    // add all pins to interrupts.
    writeRegister(tca8418_dev, REG_GPIO_INT_EN1, 0xFF);
    writeRegister(tca8418_dev, REG_GPIO_INT_EN2, 0xFF);
    writeRegister(tca8418_dev, REG_GPIO_INT_EN3, 0xFF);

    // configure rows and columns registers
    writeRegister(tca8418_dev, REG_KEY_PRESS_GPIO1, ROW_COL_BIT_MASK[MIN(tca8418_dev->rows, sizeof(ROW_COL_BIT_MASK)) - 1]);
    writeRegister(tca8418_dev, REG_KEY_PRESS_GPIO2, ROW_COL_BIT_MASK[MAX(tca8418_dev->cols, sizeof(ROW_COL_BIT_MASK)) - 1]);
    if (tca8418_dev->cols > 8)
    {
        writeRegister(tca8418_dev, REG_KEY_PRESS_GPIO3, ROW_COL_BIT_MASK[tca8418_dev->cols - 9]);
    }

    writeRegister(tca8418_dev, REG_DEBOUNCE_DIS1, 0x00);
    writeRegister(tca8418_dev, REG_DEBOUNCE_DIS2, 0x00);
    writeRegister(tca8418_dev, REG_DEBOUNCE_DIS3, 0x00);

    if (tca8418_dev->notify_pin != GPIO_NUM_NC)
    {
        gpio_config_t cfg =
            {
                .pin_bit_mask = (uint64_t)(1 << tca8418_dev->notify_pin),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_NEGEDGE,
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
                .hys_ctrl_mode = GPIO_HYS_SOFT_DISABLE
#endif
            };
        ESP_ERROR_CHECK(gpio_config(&cfg));
    }

    return tca8418_dev;
}

void tca8418_delete(tca8418_dev_t *tca8418_dev)
{
    if (tca8418_dev != NULL)
    {
        ESP_ERROR_CHECK(i2c_bus_device_delete(tca8418_dev->dev_handle));
        ESP_ERROR_CHECK(i2c_bus_delete(tca8418_dev->bus_handle));
    }
    free(tca8418_dev);
}

uint8_t tca8418_get_event_count(tca8418_dev_t *tca8418_dev)
{
    return (readRegister(tca8418_dev, REG_KEY_LOCK_EVT_COUNT) & 0x0F);
}

uint8_t tca8418_get_key(tca8418_dev_t *tca8418_dev)
{
    return readRegister(tca8418_dev, REG_KEY_EVENT_A);
}

sKeyAndChar tca8418_get_char(tca8418_dev_t *tca8418_dev)
{
    sKeyAndChar keyAndChar;

    keyAndChar.key = readRegister(tca8418_dev, REG_KEY_EVENT_A);
    keyAndChar.is_pressed = keyAndChar.key >> 7;    // read bit7, 1 is pressed, 0 is released
    keyAndChar.key = keyAndChar.key & 0x7F;         // remove bit7
    if (keyAndChar.key > 0 && keyAndChar.key <= 80)  // we can map keys 1-80
    {
        keyAndChar.ch = keyIUE[keyAndChar.key - 1];
    }

    ESP_LOGV(TAG, "keyboard key: %u, char: %c, pressed: %u", keyAndChar.key, keyAndChar.ch, keyAndChar.is_pressed);

    return keyAndChar;
}

void tca8418_flush(tca8418_dev_t *tca8418_dev)
{
    while (tca8418_get_key(tca8418_dev) != 0)
    {
        ;
    }
    readRegister(tca8418_dev, REG_GPIO_INT_STAT1);
    readRegister(tca8418_dev, REG_GPIO_INT_STAT2);
    readRegister(tca8418_dev, REG_GPIO_INT_STAT3);
    writeRegister(tca8418_dev, REG_INTERRUPT_STATUS, 0x03);
}

/// Reads a single register from the TCA8418 IC.
///
/// @param reg Register to read the value of.
///
/// @return register value.
static uint8_t readRegister(tca8418_dev_t *tca8418_dev, uint8_t reg)
{
    uint8_t receive_buf[1] = {0};
    ESP_LOGV(TAG, "Reading register %02x from dev %02x", reg, tca8418_dev->i2c_address);
    ESP_ERROR_CHECK(i2c_bus_read_byte(tca8418_dev->dev_handle, reg, receive_buf));
    return receive_buf[0];
}

/// Writes a register value to the TCA8418 IC.
///
/// @param reg Register to write a value to.
/// @param value Value to write to the register.
static void writeRegister(tca8418_dev_t *tca8418_dev, uint8_t reg, uint8_t value)
{
    ESP_LOGV(TAG, "Writing %02x to register %02x on dev %02x", value, reg, tca8418_dev->i2c_address);
    ESP_ERROR_CHECK(i2c_bus_write_byte(tca8418_dev->dev_handle, reg, value));
}
