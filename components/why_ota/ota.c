#include "ota.h"

#include "esp_ota_ops.h"
#include "esp_log.h"

#define TAG "why_ota"

struct ota_session_t {
    esp_partition_t *configured;
    esp_partition_t *running;
    esp_partition_t *update_partition;
    esp_ota_handle_t update_handle;
};

ota_handle_t badgevms_ota_session_open() {
    esp_err_t err;
    struct ota_session_t *session = (struct ota_session_t*)malloc(sizeof(ota_session_t));;

    session->configured = esp_ota_get_boot_partition();
    session->running = esp_ota_get_running_partition();
    session->update_handle = 0;

    ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08"PRIx32", running from offset 0x%08"PRIx32,
             session->configured->address, session->running->address);

    session->update_partition = esp_ota_get_next_update_partition(NULL);

    if(session->update_partition == NULL){
        return NULL;
    }

    err = esp_ota_begin(session->update_partition, OTA_WITH_SEQUENTIAL_WRITES, &(session->update_handle));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        esp_ota_abort(session->update_handle);
        free(session);
        return NULL;
    }


    return (ota_handle_t)session;
}

bool badgevms_ota_write(ota_handle_t session, void *buffer, int block_size) {
    esp_err_t err;
    err = esp_ota_write(session->update_handle, (const void *)buffer, block_size);
    if (err != ESP_OK) {
        esp_ota_abort(session->update_handle);
        free(session);
        return false;
    }
    return false;
}

bool badgevms_ota_session_commit(ota_handle_t session) {
    esp_err_t err;
    err = esp_ota_end(session->update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        return false;
    }

    err = esp_ota_set_boot_partition(session->update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return false;
    }

    free(session);
    return true;
}

bool badgevms_ota_session_abort(ota_handle_t session){
    esp_ota_abort(session->update_handle);
    free(session);
    return true;
}