#ifndef BME69X_I2C_HELPER_H
#define BME69X_I2C_HELPER_H

#include "bme69x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "i2c_bus.h"

/*!
 *  @brief Function to init the interface with I2C.
 *
 *  @param[in] bme      : Structure instance of bme69x_dev
 *  @param[in] intf     : Interface selection parameter
 *  @param[in] dev_addr : Device address (I2C address or SPI CS pin)
 *  @param[in] bus_inst : I2C bus handle (for I2C)
 *
 *  @return Status of execution
 *  @retval 0 -> Success
 *  @retval < 0 -> Failure Info
 */
int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t dev_addr, i2c_bus_handle_t bus_inst);

/*!
 *  @brief Function for reading the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] len          : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

/*!
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose value is to be written.
 *  @param[in] len          : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

/*!
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 *
 *  @param[in] period       : The required wait time in microsecond.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return void.
 *
 */
void bme69x_delay_us(uint32_t period, void *intf_ptr);

/*!
 *  @brief Prints the execution status of the APIs.
 *
 *  @param[in] api_name : Name of the API whose execution status has to be printed.
 *  @param[in] rslt     : Error code returned by the API whose execution status has to be printed.
 *
 *  @return void.
 */
void bme69x_check_rslt(const char api_name[], int8_t rslt);

/*!
 *  @brief Deinitializes coines platform
 *
 *  @return esp_err_t.
 */
esp_err_t bme69x_i2c_deinit(void);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /* BME69X_I2C_HELPER_H */