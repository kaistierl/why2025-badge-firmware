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

#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
#error "BadgeVMS requires esp-idf 5.50 (or maybe later, who knows)"
#endif

#include "badgevms/device.h"
#include "compositor/compositor_private.h"
#include "device_private.h"
#include "drivers/badgevms_i2c_bus.h"
#include "drivers/bosch_bmi270.h"
#include "drivers/fatfs.h"
#include "drivers/st7703.h"
#include "drivers/tca8418.h"
#include "drivers/tty.h"
#include "drivers/wifi.h"
#include "elf_symbols.h"
#include "esp_debug_helpers.h"
#include "esp_log.h"
#include "esp_mmu_map.h"
#include "esp_private/panic_internal.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "hal/cache_types.h"
#include "hal/mmu_hal.h"
#include "hal/mmu_ll.h"
#include "hal/mmu_types.h"
#include "logical_names.h"
#include "memory.h"
#include "ota/ota_private.h"
#include "task.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

extern void spi_flash_enable_interrupts_caches_and_other_cpu(void);
extern void spi_flash_disable_interrupts_caches_and_other_cpu(void);
extern void __real_esp_panic_handler(panic_info_t *info);
extern void SDL_CreateWindow();

static void const *__keep_symbol_elf __attribute__((used)) = &elf_find_sym;
static void const *__keep_symbol_sdl __attribute__((used)) = &SDL_CreateWindow;

static char const *TAG = "why2025_main";

extern FILE  *why_fopen(char const *pathname, char const *mode);
extern int    why_fseek(FILE *stream, long offset, int whence);
extern void   why_rewind(FILE *stream);
extern size_t why_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int    why_fclose(FILE *f);
extern long   why_ftell(FILE *stream);

#include "nvs_flash.h"

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
    if (xTaskGetApplicationTaskTag(NULL) == (void *)0x12345678) {
        task_info_t *task_info = get_task_info();
        if (task_info && task_info->pid) {
            esp_rom_printf("Crashing in task: %u\n", task_info->pid);
        } else {
            esp_rom_printf("Crashing in BadgeVMS\n");
        }

        dump_mmu();
    } else {
        esp_rom_printf("Crashing in ESP-IDF task\n");
    }

    __real_esp_panic_handler(info);
}

#include "lwip/netdb.h"

int app_main(void) {
    printf("Sizeof struct addrinfo %u\n", sizeof(struct addrinfo));

    printf("BadgeVMS Initializing...\n");
    size_t free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGW(TAG, "Free main memory: %zi", free_ram);

    memory_init();
    task_init();
    device_init();
    logical_names_system_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    device_register("KEYBOARD0", tca8418_keyboard_create());
    device_register("PANEL0", st7703_create());
    device_register("TT01", tty_create(true, true));
    device_register("FLASH0", fatfs_create_spi("FLASH0", "storage", true));
    device_register("I2CBUS0", badgevms_i2c_bus_create("I2CBUS0", 0, 400 * 1000));
    device_register("WIFI0", wifi_create());
    device_register("ORIENTATION0", bosch_bmi270_sensor_create());

    logical_name_set("SEARCH", "FLASH0:[SUBDIR], FLASH0:[SUBDIR.ANOTHER]", false);

    compositor_init("PANEL0", "KEYBOARD0");

    validate_ota_partition();

    printf("BadgeVMS is ready\n");
    free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGW(
        TAG,
        "Free main memory: %zi, free PSRAM pages: %zi/%zi, running processes %u",
        free_ram,
        get_free_psram_pages(),
        get_total_psram_pages(),
        get_num_tasks()
    );

    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_a_start, 5, &elf_a);
    //

    char **argv = malloc(sizeof(char *) * 5);
    argv[0]     = strdup("doom.eld");
    argv[1]     = strdup("-iwad");
    argv[2]     = strdup("FLASH0:doom1.wad");
    argv[3]     = strdup("-TIMEDEMO");
    argv[4]     = strdup("DEMO3");

    pid_t pida, pidb;

    printf("After argv stuff\n");
    free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGW(
        TAG,
        "Free main memory: %zi, free PSRAM pages: %zi/%zi, running processes %u",
        free_ram,
        get_free_psram_pages(),
        get_total_psram_pages(),
        get_num_tasks()
    );

    run_task_path("FLASH0:curl_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
    // pidb = run_task_path("FLASH0:hello.elf", 4096, TASK_TYPE_ELF_PATH, 2, argv);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    // pidb       = run_task(framebuffer_test_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);

    while (1) {
        while (get_num_tasks() < 2) {
            // pidb = run_task_path("FLASH0:wifi_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
            // pidb = run_task_path("FLASH0:doom.elf", 4096, TASK_TYPE_ELF, 5, argv);
            // pidb = run_task_path("FLASH0:doom.elf", 4096, TASK_TYPE_ELF, 5, argv);
            // pidb = run_task_path("FLASH0:doom.elf", 4096, TASK_TYPE_ELF, 3, argv);
            // pidb = run_task_path("FLASH0:framebuffer_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
            // pidb = run_task_path("FLASH0:hardware_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
            // pidb = run_task_path("FLASH0:sdl_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
            pidb = run_task_path("FLASH0:bmi270_test.elf", 4096, TASK_TYPE_ELF, 2, argv);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGW(
            TAG,
            "Free main memory: %zi, free PSRAM pages: %zi/%zi, running processes %u",
            free_ram,
            get_free_psram_pages(),
            get_total_psram_pages(),
            get_num_tasks()
        );
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "How did we get here?");
    // pid_t pidb = run_task(test_elf_b_start, 4096, TASK_TYPE_ELF_ROM, 0, NULL);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a);
    //    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b);

#if 0
    while (1) {
        free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGW(TAG, "Free main memory: %zi", free_ram);
        // fprintf(stderr, ".");
        // fflush(stderr);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    esp_restart();
#endif

#if 0
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Suspending worker task A\n");
    vTaskSuspend(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Restarting worker task A\n");
    vTaskResume(elf_a);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Killing worker task A\n");
    vTaskDelete(elf_a);

 //   vTaskDelay(5000 / portTICK_PERIOD_MS);
 //   printf("Killing worker task B\n");
 //   vTaskDelete(elf_b);

    printf("Sleeping for a bit\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Bye cruel workd!\n");
#endif

    return 0;
}
