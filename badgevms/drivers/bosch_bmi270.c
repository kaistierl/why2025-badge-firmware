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

#include "bosch_bmi270.h"

#include "badgevms_config.h"
#include "bmi270.h"
#include "esp_log.h"

#include <stdatomic.h>

#include <math.h>

#define BMI270_I2C_ADDR 0x69

#define UE_SW_I2C 0

#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_SCL_IO 20

#define I2C_MASTER_NUM     I2C_NUM_0           /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ I2C0_MASTER_FREQ_HZ /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT 100                 /*!< I2C master timeout in milliseconds */

#define GRAVITY_EARTH (9.80665f)

/*! Macros to select the sensors                   */
#define ACCEL UINT8_C(0x00)
#define GYRO  UINT8_C(0x01)


#define TAG "BMI270"

static i2c_bus_handle_t i2c_bus;

typedef struct {
    orientation_device_t device;
    bmi270_handle_t      sensor;
    atomic_flag          open;
    float                degrees;
} bosch_bmi270_device_t;

static int8_t set_accel_gyro_config(struct bmi2_dev *bmi);
static float  lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);
static float  lsb_to_dps(int16_t val, float dps, uint8_t bit_width);
void          why_bmi2_error_codes_print_result(int8_t rslt);
float         tilt_angle_deg(float ax, float ay);

/*!
 *  @brief Prints the execution status of the APIs.
 */
void why_bmi2_error_codes_print_result(int8_t rslt) {
    switch (rslt) {
        case BMI2_OK:

            /* Do nothing */
            break;

        case BMI2_W_FIFO_EMPTY: ESP_LOGW(TAG, "Warning [%d] : FIFO empty", rslt); break;
        case BMI2_W_PARTIAL_READ: ESP_LOGW(TAG, "Warning [%d] : FIFO partial read", rslt); break;
        case BMI2_E_NULL_PTR:
            ESP_LOGW(
                TAG,
                "Error [%d] : Null pointer error. It occurs when the user tries to assign value (not address) to a "
                "pointer,"
                " which has been initialized to NULL.",
                rslt
            );
            break;

        case BMI2_E_COM_FAIL:
            ESP_LOGW(
                TAG,
                "Error [%d] : Communication failure error. It occurs due to read/write operation failure and also due "
                "to power failure during communication",
                rslt
            );
            break;

        case BMI2_E_DEV_NOT_FOUND:
            ESP_LOGW(
                TAG,
                "Error [%d] : Device not found error. It occurs when the device chip id is incorrectly read",
                rslt
            );
            break;

        case BMI2_E_INVALID_SENSOR:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid sensor error. It occurs when there is a mismatch in the requested feature with "
                "the "
                "available one",
                rslt
            );
            break;

        case BMI2_E_SELF_TEST_FAIL:
            ESP_LOGW(
                TAG,
                "Error [%d] : Self-test failed error. It occurs when the validation of accel self-test data is "
                "not satisfied",
                rslt
            );
            break;

        case BMI2_E_INVALID_INT_PIN:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid interrupt pin error. It occurs when the user tries to configure interrupt pins "
                "apart from INT1 and INT2",
                rslt
            );
            break;

        case BMI2_E_OUT_OF_RANGE:
            ESP_LOGW(
                TAG,
                "Error [%d] : Out of range error. It occurs when the data exceeds from filtered or unfiltered data "
                "from "
                "fifo and also when the range exceeds the maximum range for accel and gyro while performing FOC",
                rslt
            );
            break;

        case BMI2_E_ACC_INVALID_CFG:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid Accel configuration error. It occurs when there is an error in accel "
                "configuration"
                " register which could be one among range, BW or filter performance in reg address 0x40",
                rslt
            );
            break;

        case BMI2_E_GYRO_INVALID_CFG:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid Gyro configuration error. It occurs when there is a error in gyro configuration"
                "register which could be one among range, BW or filter performance in reg address 0x42",
                rslt
            );
            break;

        case BMI2_E_ACC_GYR_INVALID_CFG:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid Accel-Gyro configuration error. It occurs when there is a error in accel and gyro"
                " configuration registers which could be one among range, BW or filter performance in reg address 0x40 "
                "and 0x42",
                rslt
            );
            break;

        case BMI2_E_CONFIG_LOAD:
            ESP_LOGW(
                TAG,
                "Error [%d] : Configuration load error. It occurs when failure observed while loading the "
                "configuration "
                "into the sensor",
                rslt
            );
            break;

        case BMI2_E_INVALID_PAGE:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid page error. It occurs due to failure in writing the correct feature "
                "configuration "
                "from selected page",
                rslt
            );
            break;

        case BMI2_E_SET_APS_FAIL:
            ESP_LOGW(
                TAG,
                "Error [%d] : APS failure error. It occurs due to failure in write of advance power mode configuration "
                "register",
                rslt
            );
            break;

        case BMI2_E_AUX_INVALID_CFG:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid AUX configuration error. It occurs when the auxiliary interface settings are not "
                "enabled properly",
                rslt
            );
            break;

        case BMI2_E_AUX_BUSY:
            ESP_LOGW(
                TAG,
                "Error [%d] : AUX busy error. It occurs when the auxiliary interface buses are engaged while "
                "configuring"
                " the AUX",
                rslt
            );
            break;

        case BMI2_E_REMAP_ERROR:
            ESP_LOGW(
                TAG,
                "Error [%d] : Remap error. It occurs due to failure in assigning the remap axes data for all the axes "
                "after change in axis position",
                rslt
            );
            break;

        case BMI2_E_GYR_USER_GAIN_UPD_FAIL:
            ESP_LOGW(
                TAG,
                "Error [%d] : Gyro user gain update fail error. It occurs when the reading of user gain update status "
                "fails",
                rslt
            );
            break;

        case BMI2_E_SELF_TEST_NOT_DONE:
            ESP_LOGW(
                TAG,
                "Error [%d] : Self-test not done error. It occurs when the self-test process is ongoing or not "
                "completed",
                rslt
            );
            break;

            break;

        case BMI2_E_CRT_ERROR:
            ESP_LOGW(TAG, "Error [%d] : CRT error. It occurs when the CRT test has failed", rslt);
            break;

        case BMI2_E_ST_ALREADY_RUNNING:
            ESP_LOGW(
                TAG,
                "Error [%d] : Self-test already running error. It occurs when the self-test is already running and "
                "another has been initiated",
                rslt
            );
            break;

        case BMI2_E_CRT_READY_FOR_DL_FAIL_ABORT:
            ESP_LOGW(
                TAG,
                "Error [%d] : CRT ready for download fail abort error. It occurs when download in CRT fails due to "
                "wrong "
                "address location",
                rslt
            );
            break;

        case BMI2_E_DL_ERROR:
            ESP_LOGW(
                TAG,
                "Error [%d] : Download error. It occurs when write length exceeds that of the maximum burst length",
                rslt
            );
            break;

        case BMI2_E_PRECON_ERROR:
            ESP_LOGW(
                TAG,
                "Error [%d] : Pre-conditional error. It occurs when precondition to start the feature was not "
                "completed",
                rslt
            );
            break;

        case BMI2_E_ABORT_ERROR:
            ESP_LOGW(TAG, "Error [%d] : Abort error. It occurs when the device was shaken during CRT test", rslt);
            break;

        case BMI2_E_WRITE_CYCLE_ONGOING:
            ESP_LOGW(
                TAG,
                "Error [%d] : Write cycle ongoing error. It occurs when the write cycle is already running and another "
                "has been initiated",
                rslt
            );
            break;

        case BMI2_E_ST_NOT_RUNING:
            ESP_LOGW(
                TAG,
                "Error [%d] : Self-test is not running error. It occurs when self-test running is disabled while it's "
                "running",
                rslt
            );
            break;

        case BMI2_E_DATA_RDY_INT_FAILED:
            ESP_LOGW(
                TAG,
                "Error [%d] : Data ready interrupt error. It occurs when the sample count exceeds the FOC sample limit "
                "and data ready status is not updated",
                rslt
            );
            break;

        case BMI2_E_INVALID_FOC_POSITION:
            ESP_LOGW(
                TAG,
                "Error [%d] : Invalid FOC position error. It occurs when average FOC data is obtained for the wrong"
                " axes",
                rslt
            );
            break;

        default: ESP_LOGW(TAG, "Error [%d] : Unknown error code", rslt); break;
    }
}

static int bmi270_open(void *dev, path_t *path, int flags, mode_t mode) {
    return 0;
}

static int bmi270_close(void *dev, int fd) {
    if (fd)
        return -1;
    return 0;
}

static ssize_t bmi270_read(void *dev, int fd, void *buf, size_t count) {
    if (fd)
        return -1;
    return 0;
}

static ssize_t bmi270_write(void *dev, int fd, void const *buf, size_t count) {
    return -1;
}

static ssize_t bmi270_lseek(void *dev, int fd, off_t offset, int whence) {
    return -1;
}

static int get_orientation_degrees(void *dev) {
    bosch_bmi270_device_t *device = dev;

    int8_t rslt;

    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = {BMI2_ACCEL, BMI2_GYRO};

    /* Structure to define type of sensor and their respective data. */
    struct bmi2_sens_data sensor_data;

    float                   acc_x = 0, acc_y = 0, acc_z = 0;
    struct bmi2_sens_config config;

    if (atomic_flag_test_and_set(&device->open)) {
        // Return cached result
        return (int)device->degrees;
    }

    /* Accel and gyro configuration settings. */
    rslt = set_accel_gyro_config(device->sensor);
    why_bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* NOTE:
         * Accel and Gyro enable must be done after setting configurations
         */
        rslt = bmi2_sensor_enable(sensor_list, 2, device->sensor);
        why_bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK) {
            config.type = BMI2_ACCEL;

            /* Get the accel configurations. */
            rslt = bmi2_get_sensor_config(&config, 1, device->sensor);
            why_bmi2_error_codes_print_result(rslt);

            while (true) { // sensor takes a while to answer
                rslt = bmi2_get_sensor_data(&sensor_data, device->sensor);
                why_bmi2_error_codes_print_result(rslt);

                if ((rslt == BMI2_OK) && (sensor_data.status & BMI2_DRDY_ACC) && (sensor_data.status & BMI2_DRDY_GYR)) {
                    /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
                    acc_x           = lsb_to_mps2(sensor_data.acc.x, (float)2, device->sensor->resolution);
                    acc_y           = lsb_to_mps2(sensor_data.acc.y, (float)2, device->sensor->resolution);
                    acc_z           = lsb_to_mps2(sensor_data.acc.z, (float)2, device->sensor->resolution);
                    device->degrees = tilt_angle_deg(acc_x, acc_y);
                    break;
                }
            }
        }
    }
    bmi270_sensor_disable(sensor_list, 2, device->sensor);
    atomic_flag_clear(&device->open);
    return (int)device->degrees;
}

static orientation_t get_orientation(void *dev) {
    int degrees = get_orientation_degrees(dev);
    if (degrees > 45 && degrees < 135) {
        return ORIENTATION_90;
    } else if (degrees > 135 && degrees < 225) {
        return ORIENTATION_180;
    } else if (degrees > 225 && degrees < 315) {
        return ORIENTATION_270;
    }
    return ORIENTATION_0;
}

device_t *bosch_bmi270_sensor_create() {
    bosch_bmi270_device_t *dev             = calloc(1, sizeof(bosch_bmi270_device_t));
    orientation_device_t  *orientation_dev = (orientation_device_t *)dev;
    device_t              *base_dev        = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_ORIENTATION;
    base_dev->_open  = bmi270_open;
    base_dev->_close = bmi270_close;
    base_dev->_write = bmi270_write;
    base_dev->_read  = bmi270_read;
    base_dev->_lseek = bmi270_lseek;

    orientation_dev->_get_orientation         = get_orientation;
    orientation_dev->_get_orientation_degrees = get_orientation_degrees;

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

    bmi270_i2c_config_t i2c_bmi270_conf = {
        .i2c_handle = i2c_bus,
        .i2c_addr   = BMI270_I2C_ADDR,
    };
    bmi270_sensor_create(&i2c_bmi270_conf, &dev->sensor);
    if (dev->sensor == NULL) {
        ESP_LOGE(TAG, "Failed initialising bosch bmi270 sensor");
        free(dev);
        return NULL;
    }

    ESP_LOGW(TAG, "BMI270 initialized");

    return (device_t *)dev;
}

/*!
 * @brief This internal API is used to set configurations for accel and gyro.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi) {
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Structure to define accelerometer and gyro configuration. */
    struct bmi2_sens_config config[2];

    /* Configure the type of feature. */
    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type  = BMI2_GYRO;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi2_get_sensor_config(config, 2, bmi);
    why_bmi2_error_codes_print_result(rslt);

    /* Map data ready interrupt to interrupt pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi);
    why_bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;

        /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
         * if it is set to 2, then 2^(bandwidth parameter) samples
         * are averaged, resulting in 4 averaged samples.
         * Note1 : For more information, refer the datasheet.
         * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
         * this has an adverse effect on the power consumed.
         */
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;

        /* Enable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         * For more info refer datasheet.
         */
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;

        /* Gyroscope Angular Rate Measurement Range.By default the range is 2000dps. */
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;

        /* Gyroscope bandwidth parameters. By default the gyro bandwidth is in normal mode. */
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

        /* Enable/Disable the noise performance mode for precision yaw rate sensing
         * There are two modes
         *  0 -> Ultra low power mode(Default)
         *  1 -> High performance mode
         */
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

        /* Enable/Disable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         */
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        /* Set the accel and gyro configurations. */
        rslt = bmi2_set_sensor_config(config, 2, bmi);
        why_bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width) {
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width) {
    double power = 2;

    float half_scale = (float)((pow((double)power, (double)bit_width) / 2.0f));

    return (dps / (half_scale)) * (val);
}

// Returns tilt angle in degrees [0,360), based solely on ax and ay.
// 0° corresponds to gravity along +Y, 90° along +X, etc.
// If both ax and ay are (near) zero, returns NAN.
float tilt_angle_deg(float ax, float ay) {
    if (fabs(ax) < 1e-12 && fabs(ay) < 1e-12) {
        return NAN; // direction undefined
    }
    float angle = atan2(ax, ay) * 180.0 / M_PI; // note: (ax, ay) to keep 0° at +Y
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}
