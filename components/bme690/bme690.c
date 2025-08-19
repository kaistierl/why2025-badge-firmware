#include "bme690.h"

const static char *TAG = "bme69x";

esp_err_t bme69x_sensor_create(const bme69x_i2c_config_t *i2c_conf, bme69x_handle_t *handle_ret)
{
    esp_err_t ret = ESP_OK;
    int8_t rslt;

    struct bme69x_dev *bme = (struct bme69x_dev *)calloc(1, sizeof(struct bme69x_dev));
    ESP_RETURN_ON_FALSE(bme, ESP_ERR_NO_MEM, TAG, "memory allocation for device handler failed");

    rslt = bme69x_interface_init(bme, BME69X_I2C_INTF, i2c_conf->i2c_addr, i2c_conf->i2c_handle);
    bme69x_check_rslt("bme69x_sensor_create", rslt);
    ESP_LOGI(TAG, "bme69x_check_rslt done");

    ESP_GOTO_ON_FALSE((BME69X_OK == rslt), ESP_ERR_INVALID_STATE, err, TAG, "bme69x_interface_init failed");

    // Initialize BME69X
    rslt = bme69x_init(bme);
    ESP_GOTO_ON_FALSE((rslt == BME69X_OK), ESP_ERR_INVALID_STATE, err, TAG, "bme69x_init failed");

    ESP_LOGI(TAG, " Create %-15s", "BME69X");

    *handle_ret = bme;
    return ret;

    err:
    bme69x_sensor_del(bme);
    return ret;
}

esp_err_t bme69x_sensor_del(bme69x_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");
    free(handle);

    return ESP_OK;
}
