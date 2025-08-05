#include "badgevms/misc_funcs.h"
#include "badgevms/process.h"
#include "badgevms/wifi.h"
#include "curl/curl.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#define VERSION_FILE        "FLASH0:hello_version.txt"
#define ELF_FILE            "FLASH0:hello.elf"
#define LATEST_VERSION_URL  "https://badge.why2025.org/api/v3/project-latest-revisions/badgehub_dev"
#define DOWNLOAD_URL_FORMAT "https://badge.why2025.org/api/v3/projects/badgehub_dev/rev%d/files/hello.elf"
#define VERBOSE 0
#define CHECK_INTERVAL_SECONDS 1
// Struct to hold memory buffer for curl responses
struct MemoryStruct {
    char  *memory;
    size_t size;
};

// Callback function for writing data from curl to a memory buffer
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t               realsize = size * nmemb;
    struct MemoryStruct *mem      = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size              += realsize;
    mem->memory[mem->size]  = 0;

    return realsize;
}

// Callback function for writing data from curl to a file
static size_t WriteFileCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

// Function to get the currently installed version
int get_local_version() {
    FILE *f = fopen(VERSION_FILE, "r");
    if (!f) {
        return -1; // File doesn't exist
    }
    char version_str[16];
    fgets(version_str, sizeof(version_str), f);
    fclose(f);
    return atoi(version_str);
}

pid_t pid = -1;


void restart_app(void) {
    char *name = "hello";
    char *path = "FLASH0:hello.elf";
    printf("Starting %s (%s)\n", name, path);
    if (pid != -1) {
        process_kill(pid);
    }
    pid = process_create(path, 8192, 0, NULL);
    if (pid == -1) {
        printf("Failed to start %s (%s)\n", name, path);
    }
}

int main(int argc, char *argv[]) {
    printf("HELLO_FLASHER: Starting hello_flasher app...\n");
    restart_app();
    // Connect to WiFi
    printf("HELLO_FLASHER: Connecting to WiFi...\n");
    wifi_connect();
    printf("HELLO_FLASHER: WiFi connected.\n");

    // Initialize curl
    curl_global_init(0);

    while (1) {
        CURL               *curl_handle;
        CURLcode            res;
        struct MemoryStruct chunk;

        chunk.memory = malloc(1); // will be grown as needed by the callback
        chunk.size   = 0;         // no data at this point

        curl_handle = curl_easy_init();
        if (curl_handle) {
            // Get the latest version from the server
            curl_easy_setopt(curl_handle, CURLOPT_URL, LATEST_VERSION_URL);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

            if (VERBOSE) printf("HELLO_FLASHER: Checking for new version at %s\n", LATEST_VERSION_URL);
            res = curl_easy_perform(curl_handle);

            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                long http_code = 0;
                curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
                if (http_code == 200) {
                    int remote_version = atoi(chunk.memory);
                    int local_version  = get_local_version();
                    if (VERBOSE) printf("HELLO_FLASHER: Server version: %d, Local version: %d\n", remote_version, local_version);

                    if (remote_version > local_version) {
                        printf("HELLO_FLASHER: New version available. Downloading...\n");

                        // Construct download URL
                        char download_url[256];
                        snprintf(download_url, sizeof(download_url), DOWNLOAD_URL_FORMAT, remote_version);

                        // Download the new ELF file
                        FILE *fp = fopen(ELF_FILE, "wb");
                        if (fp) {
                            curl_easy_setopt(curl_handle, CURLOPT_URL, download_url);
                            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteFileCallback);
                            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
                            res = curl_easy_perform(curl_handle);
                            fclose(fp);

                            if (res == CURLE_OK) {
                                long download_http_code = 0;
                                curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &download_http_code);
                                if (download_http_code == 200) {
                                    printf("HELLO_FLASHER: Download successful. Updating version file.\n");
                                    // Update the version file
                                    fp = fopen(VERSION_FILE, "w");
                                    if (fp) {
                                        fprintf(fp, "%d", remote_version);
                                        fclose(fp);
                                        printf("HELLO_FLASHER: Version file updated. Restarting app...\n");
                                        restart_app(); // Reboot the badge
                                    } else {
                                        printf("HELLO_FLASHER: Failed to open version file for writing.\n");
                                    }
                                } else {
                                    fprintf(stderr, "Download failed with HTTP status code: %ld\n", download_http_code);
                                }
                            } else {
                                fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
                            }
                        } else {
                            printf("HELLO_FLASHER: Failed to open file for writing: %s\n", ELF_FILE);
                        }
                    } else {
                        if (VERBOSE) printf("HELLO_FLASHER: Already on the latest version.\n");
                    }
                } else {
                    printf("HELLO_FLASHER: Server returned HTTP code %ld\n", http_code);
                }
            }

            curl_easy_cleanup(curl_handle);
            free(chunk.memory);
        }

        // Wait for 2 seconds before checking again
        if (VERBOSE)  printf("HELLO_FLASHER: Sleeping for %d seconds...\n", CHECK_INTERVAL_SECONDS);
        usleep(CHECK_INTERVAL_SECONDS * 1000 * 1000);
    }
}
