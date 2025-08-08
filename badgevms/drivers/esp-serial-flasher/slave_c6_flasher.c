#include "slave_c6_flasher.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp32_port.h"
#include "esp_loader.h"
#include "esp_log.h"
#include "why2025_firmware.h"
#include "why_io.h"

static char const *TAG = "slave_c6_flasher";

esp_err_t flash_slave_c6_if_needed() {
    why2025_binaries_t bin;

    loader_esp32_config_t const config = {
        .baud_rate         = 115200,
        .uart_port         = UART_NUM_1,
        .uart_rx_pin       = GPIO_NUM_15,
        .uart_tx_pin       = GPIO_NUM_16,
        .reset_trigger_pin = GPIO_NUM_12,
        .gpio0_trigger_pin = GPIO_NUM_13,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return ESP_FAIL;
    }

    if (connect_to_target_with_stub(115200, 3686400) == ESP_LOADER_SUCCESS) {
        target_chip_t target = esp_loader_get_target();
        ESP_LOGW(TAG, "Target = %d", target);
        if (target != ESP32C6_CHIP) {
            ESP_LOGE(TAG, "wrong target, expecting ESP32C6_CHIP");
            return ESP_FAIL;
        }

        if (!get_why2025_binaries(&bin)) {
            ESP_LOGW(TAG, "Couldn't open firmware files, skipping");
            goto out;
        }

        if (esp_loader_flash_verify_known_md5(bin.boot.addr, bin.boot.size, bin.boot.md5) != ESP_LOADER_SUCCESS) {
            ESP_LOGW(TAG, "Bootloader MD5 mismatch, flashing...");
            flash_binary(bin.boot.fp, bin.boot.size, bin.boot.addr);
        } else {
            ESP_LOGW(TAG, "Bootloader MD5 match, skipping...");
        }

        if (esp_loader_flash_verify_known_md5(bin.part.addr, bin.part.size, bin.part.md5) != ESP_LOADER_SUCCESS) {
            ESP_LOGW(TAG, "Partition table MD5 mismatch, flashing...");
            flash_binary(bin.part.fp, bin.part.size, bin.part.addr);
        } else {
            ESP_LOGW(TAG, "Partition table MD5 match, skipping...");
        }

        if (esp_loader_flash_verify_known_md5(bin.app.addr, bin.app.size, bin.app.md5) != ESP_LOADER_SUCCESS) {
            ESP_LOGW(TAG, "Application MD5 mismatch, flashing...");
            flash_binary(bin.app.fp, bin.app.size, bin.app.addr);
        } else {
            ESP_LOGW(TAG, "Application MD5 match, skipping...");
        }

    out:
        ESP_LOGW(TAG, "Resetting C6!");

        esp_loader_reset_target();

        ESP_LOGW(TAG, "Done!");

        free_why2025_binaries(&bin);

        return ESP_OK;
    }

    return ESP_FAIL;
}
