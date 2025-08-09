#include "run_tests.h"

#include "badgevms/process.h"
#include "badgevms/compositor.h"
#include "badgevms/misc_funcs.h"
#include "badgevms/wifi.h"
#include "curl/curl.h"
#include "run_tests.h"
#include "test_badge.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h> // for usleep


int ping_badgehub(void) {
    wifi_connect();
    curl_global_init(0);
    CURL *curl = curl_easy_init();

    CURLcode res;
    char     pingUrl[200];
    char mac_str[18];
    get_mac_address_str(mac_str, sizeof(mac_str));

    snprintf(
        pingUrl,
        sizeof(pingUrl),
        "https://badge.why2025.org/api/v3/ping?id=%s-v1&mac=%s",
        mac_str, mac_str
    );
    curl_easy_setopt(curl, CURLOPT_URL, pingUrl);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");
    int retries = 5;

    printf("\nDoing Badgehub ping with url: %s\n", pingUrl);
    while (retries && ((res = curl_easy_perform(curl) != CURLE_OK))) {
        printf("\nDoing Badgehub ping with url Retry[%d]: %s\n", 11 - retries, pingUrl);
        usleep(500*1000);
        --retries;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        printf("\nBadgehub ping failed: %s\n", curl_easy_strerror(res));
    } else {
        if (http_code != 200) {
            printf("\nBadgehub ping failed with HTTP status code: %ld\n", http_code);
            return -1; // Indicate an error
        }
        printf("\nBadgehub ping success\n");
    }

    return res;
}

void device_id_test(app_state_t *app) {
    // Run the Device ID test
    uint64_t unique_id = get_unique_id();
    snprintf(
        app->tests[8].status,
        sizeof(app->tests[8].status),
        "%08lX%08lX",
        (uint32_t)(unique_id >> 32),
        (uint32_t)unique_id
    );
    printf("Device ID: %08lX%08lX\n", (uint32_t)(unique_id >> 32), (uint32_t)unique_id);
    app->tests[8].passed = unique_id != 0;
}

void thread_ping(void *data) {
    app_state_t *app = (app_state_t *)data;
    device_id_test(app);
    // Run the Badgehub ping test
    int pingBadgeHubErr = ping_badgehub();
    if (pingBadgeHubErr == 0) {
        strcpy(app->tests[9].status, "Ping Success!");
        app->tests[9].passed = true;
    } else {
        snprintf(app->tests[9].status, sizeof(app->tests[9].status), "Badgehub Ping Error: %d", pingBadgeHubErr);
        app->tests[9].passed = false;
    }
}

void run_tests(app_state_t *app, int fb_num) {
    window_present(app->window, true, NULL, 0);
    thread_create(thread_ping, app, 4096);
    window_present(app->window, true, NULL, 0);
}
