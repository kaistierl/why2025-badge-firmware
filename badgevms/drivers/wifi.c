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

#include "esp-serial-flasher/slave_c6_flasher.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "task.h"
#include "thirdparty/dlmalloc.h"
#include "why_io.h"
#include "wifi_internal.h"

#include <stdbool.h>
#include <stdint.h>

#include <string.h>
#include <sys/time.h>

#define TAG "wifi"

#define DEFAULT_SCAN_LIST_SIZE 20
#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_DISCONNECTED_BIT  BIT1
#define WIFI_FAIL_BIT          BIT2

typedef struct wifi_station {
    mac_address_t                   bssid[6];
    char                            ssid[33];
    int                             primary;
    int                             secondary;
    int8_t                          rssi;
    badgevms_wifi_auth_mode_t       authmode;
    badgevms_wifi_cipher_type_t     pairwise_cipher;
    badgevms_wifi_cipher_type_t     group_cipher;
    badgevms_wifi_connection_mode_t mode;
    bool                            wps;
} wifi_station_t;

typedef struct {
    badgevms_wifi_status_t            status;
    badgevms_wifi_connection_status_t connection_status;
    badgevms_wifi_connection_status_t connection_status_want;
    SemaphoreHandle_t                 mutex;
    wifi_station_t                    current;
    int                               num_scan_results;
    wifi_station_t                    scan_results[DEFAULT_SCAN_LIST_SIZE];
    struct timespec                   last_scan_time;
} wifi_status_t;

typedef enum {
    WIFI_COMMAND_CONNECT,
    WIFI_COMMAND_DISCONNECT,
    WIFI_COMMAND_SCAN,
} wifi_command_t;

typedef struct {
    TaskHandle_t   caller;
    wifi_command_t command;
    char           ssid[33];
    char           password[65];
} wifi_command_message_t;

static int           s_retry_num = 0;
static wifi_status_t status;
static TaskHandle_t  hermes_handle;
static QueueHandle_t hermes_queue;

static EventGroupHandle_t           wifi_event_group;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

#define MIN_SCAN_INTERVAL 60 * 1000

static badgevms_wifi_auth_mode_t esp_authmode_to_badgevms(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return BADGEVMS_WIFI_AUTH_OPEN;
        case WIFI_AUTH_WEP: return BADGEVMS_WIFI_AUTH_WEP;
        case WIFI_AUTH_WPA_PSK: return BADGEVMS_WIFI_AUTH_WPA_PSK;
        case WIFI_AUTH_WPA2_PSK: return BADGEVMS_WIFI_AUTH_WPA2_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK: return BADGEVMS_WIFI_AUTH_WPA_WPA2_PSK;
        case WIFI_AUTH_ENTERPRISE: return BADGEVMS_WIFI_AUTH_WPA2_ENTERPRISE;
        case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: // Fallthrough
        case WIFI_AUTH_WPA3_EXT_PSK:            // Fallthrough
        case WIFI_AUTH_WPA3_PSK: return BADGEVMS_WIFI_AUTH_WPA3_PSK;
        case WIFI_AUTH_WPA2_WPA3_PSK: return BADGEVMS_WIFI_AUTH_WPA2_WPA3_PSK;
        case WIFI_AUTH_WAPI_PSK: return BADGEVMS_WIFI_AUTH_WAPI_PSK;
        case WIFI_AUTH_OWE: return BADGEVMS_WIFI_AUTH_OWE;
        case WIFI_AUTH_WPA3_ENT_192: return BADGEVMS_WIFI_AUTH_WPA3_ENT_192;
        case WIFI_AUTH_DPP: return BADGEVMS_WIFI_AUTH_DPP;
        case WIFI_AUTH_WPA3_ENTERPRISE: return BADGEVMS_WIFI_AUTH_WPA3_ENTERPRISE;
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return BADGEVMS_WIFI_AUTH_WPA2_WPA3_ENTERPRISE;
        case WIFI_AUTH_WPA_ENTERPRISE: return BADGEVMS_WIFI_AUTH_WPA2_WPA3_ENTERPRISE;
        default: return BADGEVMS_WIFI_AUTH_NONE;
    }
}

static wifi_auth_mode_t badgevms_to_esp_authmode(badgevms_wifi_auth_mode_t mode) {
    switch (mode) {
        case BADGEVMS_WIFI_AUTH_OPEN: return WIFI_AUTH_OPEN;
        case BADGEVMS_WIFI_AUTH_WEP: return WIFI_AUTH_WEP;
        case BADGEVMS_WIFI_AUTH_WPA_PSK: return WIFI_AUTH_WPA_PSK;
        case BADGEVMS_WIFI_AUTH_WPA2_PSK: return WIFI_AUTH_WPA2_PSK;
        case BADGEVMS_WIFI_AUTH_WPA_WPA2_PSK: return WIFI_AUTH_WPA_WPA2_PSK;
        case BADGEVMS_WIFI_AUTH_WPA2_ENTERPRISE: return WIFI_AUTH_WPA2_ENTERPRISE;
        case BADGEVMS_WIFI_AUTH_WPA3_PSK: return WIFI_AUTH_WPA3_PSK;
        case BADGEVMS_WIFI_AUTH_WPA2_WPA3_PSK: return WIFI_AUTH_WPA2_WPA3_PSK;
        case BADGEVMS_WIFI_AUTH_WAPI_PSK: return WIFI_AUTH_WAPI_PSK;
        case BADGEVMS_WIFI_AUTH_OWE: return WIFI_AUTH_OWE;
        case BADGEVMS_WIFI_AUTH_WPA3_ENT_192: return WIFI_AUTH_WPA3_ENT_192;
        case BADGEVMS_WIFI_AUTH_DPP: return WIFI_AUTH_DPP;
        case BADGEVMS_WIFI_AUTH_WPA3_ENTERPRISE: return WIFI_AUTH_WPA3_ENTERPRISE;
        case BADGEVMS_WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return WIFI_AUTH_WPA2_WPA3_ENTERPRISE;
        case BADGEVMS_WIFI_AUTH_NONE: // Fallthrough
        default: return WIFI_AUTH_OPEN;
    }
}

static badgevms_wifi_cipher_type_t esp_cipher_to_badgevms(wifi_cipher_type_t cipher) {
    switch (cipher) {
        case WIFI_CIPHER_TYPE_NONE: return BADGEVMS_WIFI_CIPHER_TYPE_NONE;
        case WIFI_CIPHER_TYPE_WEP40: return BADGEVMS_WIFI_CIPHER_TYPE_WEP40;
        case WIFI_CIPHER_TYPE_WEP104: return BADGEVMS_WIFI_CIPHER_TYPE_WEP104;
        case WIFI_CIPHER_TYPE_TKIP: return BADGEVMS_WIFI_CIPHER_TYPE_TKIP;
        case WIFI_CIPHER_TYPE_CCMP: return BADGEVMS_WIFI_CIPHER_TYPE_CCMP;
        case WIFI_CIPHER_TYPE_TKIP_CCMP: return BADGEVMS_WIFI_CIPHER_TYPE_TKIP_CCMP;
        case WIFI_CIPHER_TYPE_AES_CMAC128: return BADGEVMS_WIFI_CIPHER_TYPE_AES_CMAC128;
        case WIFI_CIPHER_TYPE_SMS4: return BADGEVMS_WIFI_CIPHER_TYPE_SMS4;
        case WIFI_CIPHER_TYPE_GCMP: return BADGEVMS_WIFI_CIPHER_TYPE_GCMP;
        case WIFI_CIPHER_TYPE_GCMP256: return BADGEVMS_WIFI_CIPHER_TYPE_GCMP256;
        case WIFI_CIPHER_TYPE_AES_GMAC128: return BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC128;
        case WIFI_CIPHER_TYPE_AES_GMAC256: return BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC256;
        case WIFI_CIPHER_TYPE_UNKNOWN: return BADGEVMS_WIFI_CIPHER_TYPE_UNKNOWN;
        default: return BADGEVMS_WIFI_CIPHER_TYPE_UNKNOWN;
    }
}

static wifi_cipher_type_t badgevms_to_esp_cipher(badgevms_wifi_cipher_type_t cipher) {
    switch (cipher) {
        case BADGEVMS_WIFI_CIPHER_TYPE_NONE: return WIFI_CIPHER_TYPE_NONE;
        case BADGEVMS_WIFI_CIPHER_TYPE_WEP40: return WIFI_CIPHER_TYPE_WEP40;
        case BADGEVMS_WIFI_CIPHER_TYPE_WEP104: return WIFI_CIPHER_TYPE_WEP104;
        case BADGEVMS_WIFI_CIPHER_TYPE_TKIP: return WIFI_CIPHER_TYPE_TKIP;
        case BADGEVMS_WIFI_CIPHER_TYPE_CCMP: return WIFI_CIPHER_TYPE_CCMP;
        case BADGEVMS_WIFI_CIPHER_TYPE_TKIP_CCMP: return WIFI_CIPHER_TYPE_TKIP_CCMP;
        case BADGEVMS_WIFI_CIPHER_TYPE_AES_CMAC128: return WIFI_CIPHER_TYPE_AES_CMAC128;
        case BADGEVMS_WIFI_CIPHER_TYPE_SMS4: return WIFI_CIPHER_TYPE_SMS4;
        case BADGEVMS_WIFI_CIPHER_TYPE_GCMP: return WIFI_CIPHER_TYPE_GCMP;
        case BADGEVMS_WIFI_CIPHER_TYPE_GCMP256: return WIFI_CIPHER_TYPE_GCMP256;
        case BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC128: return WIFI_CIPHER_TYPE_AES_GMAC128;
        case BADGEVMS_WIFI_CIPHER_TYPE_AES_GMAC256: return WIFI_CIPHER_TYPE_AES_GMAC256;
        case BADGEVMS_WIFI_CIPHER_TYPE_UNKNOWN: return WIFI_CIPHER_TYPE_UNKNOWN;
        default: return WIFI_CIPHER_TYPE_UNKNOWN;
    }
}

static badgevms_wifi_connection_mode_t esp_phy_to_badgevms_mode(wifi_ap_record_t *ap_record) {
    uint32_t mode = 0;

    if (ap_record->phy_11b)
        mode |= WIFI_MODE_11B;
    if (ap_record->phy_11g)
        mode |= WIFI_MODE_11G;
    if (ap_record->phy_11n)
        mode |= WIFI_MODE_11N;
    if (ap_record->phy_lr)
        mode |= WIFI_MODE_LR;
    if (ap_record->phy_11a)
        mode |= WIFI_MODE_11A;
    if (ap_record->phy_11ac)
        mode |= WIFI_MODE_11AC;
    if (ap_record->phy_11ax)
        mode |= WIFI_MODE_11AX;

    return (badgevms_wifi_connection_mode_t)mode;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (status.connection_status_want != WIFI_DISCONNECTED) {
            status.connection_status = WIFI_DISCONNECTED;
            ESP_LOGW(TAG, "unexpected wifi disconnect, reconnecting");
            if (s_retry_num < 10) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGW(TAG, "retry to connect to the AP");
            } else {
                s_retry_num = 0;
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGW(TAG, "failed to connect to the AP");
                status.connection_status = WIFI_ERROR;
            }
        } else {
            ESP_LOGW(TAG, "User requested disconnect");
            xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGW(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        status.connection_status = WIFI_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void hermes_do_disconnect() {
    status.connection_status_want = WIFI_DISCONNECTED;
    if (status.connection_status == WIFI_DISCONNECTED) {
        ESP_LOGW("HERMES", "Already disconnected");
        return;
    }

    status.connection_status = WIFI_DISCONNECTED;
    int retries              = 5;
again:
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,
        pdFALSE,
        5000 / portTICK_PERIOD_MS
    );
    if ((!(bits & WIFI_DISCONNECTED_BIT)) && retries) {
        --retries;
        goto again;
    }
}

static void hermes_do_connect(char const *ssid, char const *password) {
    status.connection_status_want = WIFI_CONNECTED;
    if (status.connection_status == WIFI_CONNECTED) {
        ESP_LOGW("HERMES", "Already connected");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = { 0 };
    if (ssid && ssid[0]) {
        strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        if (password && password[0]) {
            strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        } else {
            wifi_config.sta.password[0] = '\0';
        }
    } else {
        // Fallback to previous default if no SSID provided
        strlcpy((char *)wifi_config.sta.ssid, "WHY2025-open", sizeof(wifi_config.sta.ssid));
        wifi_config.sta.password[0] = '\0';
    }


    // esp_eap_client_set_identity((uint8_t *)EXAMPLE_EAP_ID, strlen(EXAMPLE_EAP_ID));

    // esp_eap_client_set_username((unsigned char const *)"badge", 5);
    // esp_eap_client_set_password((unsigned char const *)"badge", 5);
    // esp_eap_client_set_ttls_phase2_method(ESP_EAP_TTLS_PHASE2_MSCHAPV2);
    // esp_wifi_sta_enterprise_enable();
    // ESP_ERROR_CHECK(esp_wifi_start());

    int retries = 10;
again:
    ESP_LOGW("HERMES", "Dialing...");

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK && retries) {
        --retries;
        vTaskDelay(100);
        goto again;
    }

    esp_wifi_connect();
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdTRUE,
        5000 / portTICK_PERIOD_MS
    );

    if ((!(bits & WIFI_CONNECTED_BIT)) && retries) {
        ESP_LOGW("HERMES", "Timeout, maybe Apollo is on the phone?");
        --retries;
        goto again;
    }

    ESP_LOGW("HERMES", "Mount olympus paged");
}

static void hermes_do_scan() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    long elapsed_us = (cur_time.tv_sec - status.last_scan_time.tv_sec) * 1000000L +
                      (cur_time.tv_nsec - status.last_scan_time.tv_nsec) / 1000L;

    if (elapsed_us < MIN_SCAN_INTERVAL && (status.last_scan_time.tv_sec + status.last_scan_time.tv_nsec)) {
        ESP_LOGW("HERMES", "Unwilling to scan again so soon");
        return;
    }

    status.last_scan_time = cur_time;

    esp_wifi_scan_start(NULL, true);

    uint16_t         number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t         ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    ESP_LOGW("HERMES", "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    xSemaphoreTake(status.mutex, portMAX_DELAY);
    // Limit to the number of records actually fetched into ap_info
    if (ap_count > number) {
        status.num_scan_results = number;
    } else {
        status.num_scan_results = ap_count;
    }

    int to_copy = status.num_scan_results;
    for (int i = 0; i < to_copy; i++) {
        wifi_station_t *s = &status.scan_results[i];

        memcpy(&s->bssid, ap_info[i].bssid, sizeof(mac_address_t));
        memcpy(&s->ssid, ap_info[i].ssid, sizeof(s->ssid));
        s->primary         = ap_info[i].primary;
        s->secondary       = ap_info[i].second;
        s->rssi            = ap_info[i].rssi;
        s->authmode        = esp_authmode_to_badgevms(ap_info[i].authmode);
        s->pairwise_cipher = esp_cipher_to_badgevms(ap_info[i].pairwise_cipher);
        s->group_cipher    = esp_cipher_to_badgevms(ap_info[i].group_cipher);
        s->mode            = esp_phy_to_badgevms_mode(&ap_info[i]);
        s->wps             = ap_info[i].wps;
    }
    xSemaphoreGive(status.mutex);
}

static void hermes(void *ignored) {
    ESP_LOGW("HERMES", "Starting");
    wifi_command_message_t *command;
    while (1) {
        if (xQueueReceive(hermes_queue, &command, portMAX_DELAY) == pdTRUE) {
            switch (command->command) {
                case WIFI_COMMAND_CONNECT:
                    ESP_LOGW("HERMES", "Connecting to the divine realm");
                    hermes_do_connect(command->ssid[0] ? command->ssid : NULL,
                                     command->password[0] ? command->password : NULL);
                    break;
                case WIFI_COMMAND_DISCONNECT:
                    ESP_LOGW("HERMES", "Confining to the mortal plane");
                    hermes_do_disconnect();
                    break;
                case WIFI_COMMAND_SCAN:
                    ESP_LOGW("HERMES", "Scanning for pathways to olympus");
                    hermes_do_scan();
                    break;
                default: ESP_LOGW("HERMES", "I don't know how to do %u", command->command);
            }

            if (command->caller) {
                if (eTaskGetState(command->caller) != eDeleted) {
                    xTaskNotifyIndexed(command->caller, 0, status.connection_status, eSetValueWithOverwrite);
                }
            }
            free(command);
        }
    }
}

static badgevms_wifi_connection_status_t send_command(wifi_command_t command) {
    wifi_command_message_t *c = calloc(1, sizeof(wifi_command_message_t));

    c->caller  = xTaskGetCurrentTaskHandle();
    c->command = command;

    xQueueSend(hermes_queue, &c, portMAX_DELAY);
    badgevms_wifi_connection_status_t status = ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
    return status;
}

badgevms_wifi_status_t wifi_get_status() {
    return status.status;
}

badgevms_wifi_connection_status_t wifi_get_connection_status() {
    return status.connection_status;
}

wifi_station_handle wifi_get_connection_station() {
    wifi_station_t *ret = 0;
    xSemaphoreTake(status.mutex, portMAX_DELAY);
    if (status.connection_status == WIFI_CONNECTED) {
        ret = dlmalloc(sizeof(wifi_station_t));
        if (ret) {
            memcpy(ret, &status.current, sizeof(wifi_station_t));
        }
    }
    xSemaphoreGive(status.mutex);
    return ret;
}

badgevms_wifi_connection_status_t wifi_connect() {
    badgevms_wifi_connection_status_t s;
    xSemaphoreTake(status.mutex, portMAX_DELAY);
    s = status.connection_status;
    xSemaphoreGive(status.mutex);

    if (s == WIFI_CONNECTED) {
        ESP_LOGW(TAG, "Already connected");
        return s;
    }

    return send_command(WIFI_COMMAND_CONNECT);
}

badgevms_wifi_connection_status_t wifi_disconnect() {
    badgevms_wifi_connection_status_t s;
    xSemaphoreTake(status.mutex, portMAX_DELAY);
    s = status.connection_status;
    xSemaphoreGive(status.mutex);

    if (s == WIFI_DISCONNECTED) {
        return s;
    }

    return send_command(WIFI_COMMAND_DISCONNECT);
}

int wifi_scan_get_num_results() {
    if (status.status != WIFI_DISABLED) {
        // For scans, the connection status isn't relevant; ignore return code
        (void)send_command(WIFI_COMMAND_SCAN);
    }

    xSemaphoreTake(status.mutex, portMAX_DELAY);
    int ret = status.num_scan_results;
    xSemaphoreGive(status.mutex);
    return ret;
}

wifi_station_handle wifi_scan_get_result(int num) {
    wifi_station_t *ret = NULL;
    xSemaphoreTake(status.mutex, portMAX_DELAY);
    if (num >= status.num_scan_results) {
        goto out;
    }

    ret = dlmalloc(sizeof(wifi_station_t));
    if (ret) {
        memcpy(ret, &status.scan_results[num], sizeof(wifi_station_t));
    }
out:
    xSemaphoreGive(status.mutex);

    return ret;
}

void wifi_scan_free_station(wifi_station_handle station) {
    dlfree(station);
}

char const *wifi_station_get_ssid(wifi_station_handle station) {
    return (char const *)station->ssid;
}

mac_address_t *wifi_station_get_bssid(wifi_station_handle station) {
    return station->bssid;
}

badgevms_wifi_auth_mode_t wifi_station_get_mode(wifi_station_handle station) {
    return station->mode;
}

int wifi_station_get_primary_channel(wifi_station_handle station) {
    return station->primary;
}

int wifi_station_get_secondary_channel(wifi_station_handle station) {
    return station->secondary;
}

int wifi_station_get_rssi(wifi_station_handle station) {
    return station->rssi;
}

bool wifi_station_wps(wifi_station_handle station) {
    return station->wps;
}

badgevms_wifi_auth_mode_t wifi_station_get_authmode(wifi_station_handle station) {
    return station->authmode;
}

badgevms_wifi_connection_status_t wifi_connect_to(char const *ssid, char const *password) {
    if (!ssid || !ssid[0]) {
        return WIFI_ERROR;
    }

    wifi_command_message_t *c = calloc(1, sizeof(wifi_command_message_t));
    if (!c) return WIFI_ERROR;
    c->caller  = xTaskGetCurrentTaskHandle();
    c->command = WIFI_COMMAND_CONNECT;
    strlcpy(c->ssid, ssid, sizeof(c->ssid));
    if (password) strlcpy(c->password, password, sizeof(c->password));

    xQueueSend(hermes_queue, &c, portMAX_DELAY);
    badgevms_wifi_connection_status_t res = ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
    return res;
}

static int wifi_open(void *dev, path_t *path, int flags, mode_t mode) {
    if (path->directory || path->filename)
        return -1;
    return 0;
}

static int wifi_close(void *dev, int fd) {
    if (fd == 0)
        return 0;
    return -1;
}

static ssize_t wifi_write(void *dev, int fd, void const *buf, size_t count) {
    return 0;
}

static ssize_t wifi_read(void *dev, int fd, void *buf, size_t count) {
    return 0;
}

static ssize_t wifi_lseek(void *dev, int fd, off_t offset, int whence) {
    return (off_t)-1;
}

static void start_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id)
    );
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip)
    );

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

device_t *wifi_create() {
    ESP_LOGI(TAG, "Initializing");

    ESP_LOGI(TAG, "Flashing C6");
    flash_slave_c6_if_needed();

    wifi_device_t *dev      = malloc(sizeof(wifi_device_t));
    device_t      *base_dev = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_BLOCK;
    base_dev->_open  = wifi_open;
    base_dev->_close = wifi_close;
    base_dev->_write = wifi_write;
    base_dev->_read  = wifi_read;
    base_dev->_lseek = wifi_lseek;

    status.status            = WIFI_ENABLED;
    status.connection_status = WIFI_DISCONNECTED;

    wifi_event_group = xEventGroupCreate();

    start_wifi();

    status.mutex = xSemaphoreCreateMutex();
    hermes_queue = xQueueCreate(5, sizeof(wifi_command_message_t *));
    create_kernel_task(hermes, "Hermes", 4096, NULL, 5, &hermes_handle, 0);
    return (device_t *)dev;
}
