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

#include "compositor/compositor_private.h"
#include "device.h"
#include "drivers/fatfs.h"
#include "drivers/st7703.h"
#include "drivers/tca8418.h"
#include "drivers/tty.h"
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
#include "png.h"
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

#if 0
extern uint8_t const test_elf_a_start[] asm("_binary_test_basic_a_elf_start");
extern uint8_t const test_elf_a_end[] asm("_binary_test_basic_a_elf_end");
extern uint8_t const test_elf_b_start[] asm("_binary_test_basic_b_elf_start");
extern uint8_t const test_elf_b_end[] asm("_binary_test_basic_b_elf_end");
extern uint8_t const test_elf_c_start[] asm("_binary_test_basic_c_elf_start");
extern uint8_t const test_elf_c_end[] asm("_binary_test_basic_c_elf_end");
extern uint8_t const test_elf_shell_start[] asm("_binary_test_shell_elf_start");
extern uint8_t const test_elf_shell_end[] asm("_binary_test_shell_elf_end");
#endif
extern uint8_t const test_elf_bench_a_start[] asm("_binary_bench_basic_a_elf_start");
extern uint8_t const test_elf_bench_a_end[] asm("_binary_bench_basic_a_elf_end");
extern uint8_t const test_elf_bench_b_start[] asm("_binary_bench_basic_b_elf_start");
extern uint8_t const test_elf_bench_b_end[] asm("_binary_bench_basic_b_elf_end");

extern uint8_t const framebuffer_test_a_start[] asm("_binary_framebuffer_test_a_elf_start");
extern uint8_t const framebuffer_test_a_end[] asm("_binary_framebuffer_test_a_elf_end");

extern uint8_t const sdl_test_start[] asm("_binary_sdl_test_elf_start");
extern uint8_t const sdl_test_end[] asm("_binary_sdl_test_elf_end");

extern uint8_t const lvgl_test_start[] asm("_binary_lvgl_test_elf_start");
extern uint8_t const lvgl_test_end[] asm("_binary_lvgl_test_elf_end");

extern FILE  *why_fopen(char const *pathname, char const *mode);
extern int    why_fseek(FILE *stream, long offset, int whence);
extern void   why_rewind(FILE *stream);
extern size_t why_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int    why_fclose(FILE *f);
extern long   why_ftell(FILE *stream);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
    task_info_t *task_info = get_task_info();
    if (task_info) {
        esp_rom_printf("Crashing in task: %u\n", task_info->pid);
    } else {
        esp_rom_printf("Crashing in BadgeVMS\n");
    }

    dump_mmu();

    __real_esp_panic_handler(info);
}

int app_main(void) {
    printf("BadgeVMS Initializing...\n");
    size_t free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGW(TAG, "Free main memory: %zi", free_ram);

    memory_init();
    task_init();
    device_init();
    logical_names_system_init();

    device_register("KEYBOARD0", tca8418_keyboard_create());
    device_register("PANEL0", st7703_create());
    device_register("TT01", tty_create(true, true));
    device_register("FLASH0", fatfs_create_spi("FLASH0", "storage", true));
    logical_name_set("SEARCH", "FLASH0:[SUBDIR], FLASH0:[SUBDIR.ANOTHER]", false);

    compositor_init("PANEL0", "KEYBOARD0");

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
    //    png_image image;
    //    memset(&image, 0, (sizeof image));
    //   image.version = PNG_IMAGE_VERSION;

#if 0
    int           buf_size   = 720 * 720 * 2;
    // png_bytep pixels = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    lcd_device_t *lcd_device = (lcd_device_t *)device_get("PANEL0");
    if (lcd_device) {
        void *pixels;
        lcd_device->_getfb(lcd_device, &pixels);
        memset(pixels, 0, buf_size);
        for (int i = 0; i < buf_size / 2; ++i) {
            if (i < 720 * (720 / 3)) 
                ((uint16_t *)pixels)[i] = 0x1F;
            else if (i < 720 * ((720 / 3) * 2))
                ((uint16_t *)pixels)[i] = 0x7E0;
            else 
                ((uint16_t *)pixels)[i] = 0xF800;

        }
        lcd_device->_draw(lcd_device, 0, 0, 720, 720, pixels);
    }
#endif

#if 0
    FILE *f = why_fopen("FLASH0:BADGEVMS.PNG", "r");
    if (f) {
        why_fseek(f, 0, SEEK_END);
        long size = why_ftell(f);
        if (size > 0) {
            why_fseek(f, 0, SEEK_SET);
            void *imgbuf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM); 
            if (imgbuf) {
                ESP_LOGW(TAG, "Got buffer at %p-%pof size %lu", imgbuf, imgbuf + size, size);
                size_t s = why_fread(imgbuf, size, 1, f);
                if (s) {
                    image.format = PNG_FORMAT_RGB;
                    png_image_begin_read_from_memory(&image, imgbuf, size);
                    int stride = PNG_IMAGE_ROW_STRIDE(image);
                    int buf_size = PNG_IMAGE_SIZE(image);
                    png_bytep pixels = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
                    ESP_LOGW(TAG, "Decompressed image is %u bytes", PNG_IMAGE_SIZE(image));
                    png_image_finish_read(&image, NULL, pixels, stride, NULL);
                    memset(pixels, 0, buf_size);
                    for (int i = 0; i < buf_size; i += 3) {
                        pixels[i] = 0xff;
                    }
                    free(imgbuf);
                    why_fclose(f);
                    lcd_device_t *lcd_device = (lcd_device_t*)device_get("PANEL0");
                    lcd_device->_draw(lcd_device, 0, 0, 720, 720, pixels);
                } else {
                    ESP_LOGW(TAG, "Read failed: %s", strerror(errno));
                } 
            } else {
                ESP_LOGW(TAG, "Malloc(%lu) failed: %s", size, strerror(errno));
            }
        } else {
            ESP_LOGW(TAG, "fseek/ftell failed");
        }
    } else {
        ESP_LOGW(TAG, "Couldn't open image");
    }
#endif

    char **argv = malloc(sizeof(char *) * 2);
    argv[0]     = strdup("test_elf_c");
    argv[1]     = strdup("argv[xxx]");


    pid_t pidb = run_task(sdl_test_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
    // pid_t pidb = run_task(framebuffer_test_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    //pidb       = run_task(framebuffer_test_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
    // run_task(lvgl_test_start, 16384, TASK_TYPE_ELF_ROM, 2, argv);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);

#if 0
    while (1) {
        fprintf(stderr, ".");
        fflush(stderr);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };
#endif

    while (1) {
        while (get_num_tasks() < 0) {
            sprintf(argv[1], "argv[%d]", 0);
            // pid_t pida = run_task(test_elf_bench_a_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
            // ESP_LOGI(TAG, "Started task with pid %i", pida);
            pid_t pidb = run_task(test_elf_bench_b_start, 4096, TASK_TYPE_ELF_ROM, 2, argv);
            // ESP_LOGI(TAG, "Started task with pid %i", pidb);
            // vTaskDelay(500 / portTICK_PERIOD_MS);
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
    // pid_t pidb = run_task(test_elf_b_start, 4096, TASK_TYPE_ELF_ROM, 0, NULL);
    // ESP_LOGI(TAG, "Started task with pid %i", pidb);
    //    xTaskCreate(run_elf, "Task1", 16384, test_elf_shell_start, 5, &elf_a);
    //    xTaskCreate(run_elf, "Task2", 4096, test_elf_b_start, 5, &elf_b);

    while (1) {
        free_ram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGW(TAG, "Free main memory: %zi", free_ram);
        // fprintf(stderr, ".");
        // fflush(stderr);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    };
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    esp_restart();

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
