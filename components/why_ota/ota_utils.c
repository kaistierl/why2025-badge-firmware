#include "ota_utils.h"

#include "esp_ota_ops.h"
#include "esp_log.h"

#define TAG "why_ota_utils"

bool validate_ota_partition(){
    esp_err_t err;

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGW(TAG, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
             running->type, running->subtype, running->address);

    esp_ota_img_states_t ota_state;
    err = esp_ota_get_state_partition(running, &ota_state);
    if (err != ESP_OK) {
        return false;
    }

    if(ota_state == ESP_OTA_IMG_PENDING_VERIFY){
        ESP_LOGW(TAG, "Marking running partition as valid and cancelling rollback");
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            return false;
        }
    }
    return true;
}