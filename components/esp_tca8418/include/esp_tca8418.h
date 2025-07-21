/*
 * SPDX-FileCopyrightText: 2024 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "i2c_bus.h"
#include "hal/gpio_types.h"
#include "keymap.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TCA8418_MAX_ROW_COUNT 8          // Maximum number of rows supported by the TCA8418 IC.
#define TCA8418_MAX_COLUMN_COUNT 10      // Maximum number of columns supported by the TCA8418 IC.
#define TCA8418_I2C_DEFAULT_ADDRESS 0x34 // i2c default address of tca8418 chip

    typedef struct
    {
        gpio_num_t scl_pin;                 // GPIO pin used for I2C SCL.
        gpio_num_t sda_pin;                 // GPIO pin used for I2C SDA.
        uint8_t i2c_address;                // I2C address of the TCA8418 IC.
        gpio_num_t notify_pin;              // GPIO pin used for notification of keypress.
        uint8_t rows;                       // Number of rows to configure the TCA8418 to use, default to maximum (8).
        uint8_t cols;                       // Number of rows to configure the TCA8418 to use, default to maximum (10).
        i2c_bus_handle_t bus_handle;        // I2C bus handle
        i2c_bus_device_handle_t dev_handle; // I2C device handle to use for I2C communication.
    } tca8418_dev_t;

    typedef struct
    {
        uint8_t key;                        // a key value between 0 and 127 of wich 1-80 are the keys in the matrix and 97-114 are GPI
        char ch;                            // a valid key mapped to a character
        bool is_pressed;                    // 1 is pressed, 0 is released
    } sKeyAndChar;

    /// @brief Constructor
    ///        Attempts to detect and initializes the TCA8418 IC.
    /// @param scl_pin          GPIO pin used for I2C SCL.
    /// @param sda_pin          GPIO pin used for I2C SDA.
    /// @param i2c_address      I2C address of the TCA8418 IC.
    /// @param notify_pin       GPIO pin used for notification of keypress.
    /// @param rows             Number of rows to configure the TCA8418 to use, default to maximum (8).
    /// @param cols             Number of rows to configure the TCA8418 to use, default to maximum (10).
    /// @return                 pointer to initialized tca8418_dev_t struct after successfully creating i2c_bus i2c_device and configuring it
    tca8418_dev_t *tca8418_create(gpio_num_t scl_pin, gpio_num_t sda_pin, uint8_t i2c_address, gpio_num_t notify_pin, uint8_t rows, uint8_t cols);

    /// @brief Destructor
    /// @param tca8418_dev      pointer to initialized tca8418_dev_t struct
    void tca8418_delete(tca8418_dev_t *tca8418_dev);

    /// @return the number of pending keypress events that have yet to be read.
    uint8_t tca8418_get_event_count(tca8418_dev_t *tca8418_dev);

    /// Returns the last keypress event value.
    ///
    /// @return key code.
    ///
    /// @note this value is based on a 10x8 grid for all columns and rows even if
    /// configured to be smaller than this size.
    ///
    /// Example grid showing the values as expected to be returned from this function:
    ///
    ///            COLUMNS (0 - 9)
    /// ROW 0: 0   1  2  3  4  5  6  7  8  9
    /// ROW 1: 10 11 12 13 14 15 16 17 18 19
    /// ROW 2: 20 21 22 23 24 25 26 27 28 29
    /// ...
    /// ROW 7: 70 71 72 73 74 75 76 77 78 79
    uint8_t tca8418_get_key(tca8418_dev_t *tca8418_dev);

    sKeyAndChar tca8418_get_char(tca8418_dev_t *tca8418_dev);

    /// Discards all remaining keypress events.
    void tca8418_flush(tca8418_dev_t *tca8418_dev);

#ifdef __cplusplus
}
#endif