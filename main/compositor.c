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

#include "compositor.h"

#include "device.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"
#include "memory.h"
#include "task.h"

#include <sys/time.h>

#define TAG "compositor"

static TaskHandle_t  compositor_handle;
static QueueHandle_t compositor_queue;
static lcd_device_t *lcd_device;

typedef struct {
    framebuffer_t       framebuffer;
    allocation_range_t *pages;
    size_t              num_pages;
    uint16_t            x;
    uint16_t            y;
    task_info_t        *task_info;
    SemaphoreHandle_t   ready;
} managed_framebuffer_t;

static int fb_count = 0;

framebuffer_t *framebuffer_allocate(uint32_t w, uint32_t h) {
    task_info_t *task_info = get_task_info();
    ESP_LOGW(TAG, "Mapping framebuffer for task %u", task_info->pid);

    size_t    num_pages        = 0;
    size_t    framebuffer_size = w * h * FRAMEBUFFER_BPP;
    uintptr_t vaddr_start      = framebuffer_vaddr_allocate(framebuffer_size, &num_pages);
    if (!vaddr_start) {
        ESP_LOGE(TAG, "No vaddr space for frame buffer");
        return NULL;
    }

    managed_framebuffer_t *framebuffer = calloc(1, sizeof(managed_framebuffer_t));
    if (!framebuffer) {
        ESP_LOGE(TAG, "No kernel RAM for frame buffer container");
        framebuffer_vaddr_deallocate(vaddr_start);
        return NULL;
    }

    allocation_range_t *tail_range = NULL;
    if (!pages_allocate(vaddr_start, num_pages, &framebuffer->pages, &tail_range)) {
        ESP_LOGE(TAG, "No physical memory pages for frame buffer");
        framebuffer_vaddr_deallocate(vaddr_start);
        free(framebuffer);
        return NULL;
    }

    ESP_LOGI(
        TAG,
        "Attempting to map our vaddr %p head_range %p tail_range %p",
        (void *)framebuffer->pages->vaddr_start,
        framebuffer->pages,
        tail_range
    );
    framebuffer_map_pages(framebuffer->pages, tail_range);

    framebuffer->framebuffer.pixels = (uint16_t *)vaddr_start;
    framebuffer->framebuffer.w      = w;
    framebuffer->framebuffer.h      = h;
    framebuffer->x                  = 0;
    framebuffer->y                  = 0;
    framebuffer->task_info          = task_info;
    framebuffer->ready              = xSemaphoreCreateBinary();

    if (fb_count > 2) {
        framebuffer->x = (fb_count - 2) * w;
        framebuffer->y = (fb_count - 2) * h;
    }
    fb_count++;

    // vTaskPrioritySet(framebuffer->task_info->handle, TASK_PRIORITY_FOREGROUND);
    ESP_LOGI(
        TAG,
        "Allocated framebuffer at %p, pixels at %p, size %zi",
        framebuffer,
        framebuffer->framebuffer.pixels,
        num_pages
    );
    return (framebuffer_t *)framebuffer;
}

void framebuffer_free(framebuffer_t *framebuffer) {
    managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)framebuffer;
    framebuffer_vaddr_deallocate((uintptr_t)managed_framebuffer->framebuffer.pixels);
    pages_deallocate(managed_framebuffer->pages);
    free(managed_framebuffer);
}

void framebuffer_post(framebuffer_t *framebuffer, bool block) {
    managed_framebuffer_t *managed_framebuffer = (managed_framebuffer_t *)framebuffer;

    ESP_LOGV(TAG, "Posting framebuffer %p", framebuffer);

    // This is safe because framebuffers do not get allocated in user space
    xQueueSend(compositor_queue, &managed_framebuffer, portMAX_DELAY);

    if (block) {
        ESP_LOGV(TAG, "Suspending myself");
        xSemaphoreTake(managed_framebuffer->ready, portMAX_DELAY);
    }
}

static void IRAM_ATTR NOINLINE_ATTR compositor(void *ignored) {
    static ppa_client_handle_t ppa_srm_handle       = NULL;
    static size_t              data_cache_line_size = 0;

    long const target_frame_time_ms = 1000 / 30;
    long       sleep_time           = target_frame_time_ms;
    void      *framebuffers[2];
    lcd_device->_getfb(lcd_device, 1, &framebuffers[0]);
    lcd_device->_getfb(lcd_device, 2, &framebuffers[1]);

    memset(framebuffers[0], 0, FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP);
    memset(framebuffers[1], 0, FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP);

    bool cur_fb = true;

    ppa_srm_rotation_angle_t ppa_rotation;
    ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
    int x_offset;
    int y_offset;

    ppa_client_config_t ppa_srm_config = {
        .oper_type             = PPA_OPERATION_SRM,
        .max_pending_trans_num = 5,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    ESP_LOGI(TAG, "framebuffers[0] = %p, framebuffers[1] = %p", framebuffers[0], framebuffers[1]);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    end_time = start_time;

    while (1) {
        bool changes = true;

        managed_framebuffer_t *framebuffer;
        // Wait for framebuffers to come in
        // ESP_LOGI(TAG, "Sleeping for %li ms", sleep_time);
        if (xQueueReceive(compositor_queue, &framebuffer, sleep_time / portTICK_PERIOD_MS) == pdTRUE) {
            y_offset = FRAMEBUFFER_MAX_W - framebuffer->framebuffer.w - 1;

            // ESP_LOGI(TAG, "received framebuffers %p, pixels at %p, compositing to framebuffer %p", framebuffer,
            // framebuffer->framebuffer.pixels, framebuffers[cur_fb]); memcpy(framebuffers[cur_fb],
            // framebuffer->framebuffer.pixels, framebuffer->framebuffer.w * framebuffer->framebuffer.h * 2);
            // memset(framebuffers[cur_fb], 0xff, framebuffer->framebuffer.w * framebuffer->framebuffer.h * 2);
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            ppa_srm_oper_config_t oper_config = {
                .in.buffer         = framebuffer->framebuffer.pixels,
                .in.pic_w          = framebuffer->framebuffer.w,
                .in.pic_h          = framebuffer->framebuffer.h,
                .in.block_w        = framebuffer->framebuffer.w,
                .in.block_h        = framebuffer->framebuffer.h,
                .in.block_offset_x = 0,
                .in.block_offset_y = 0,
                .in.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = framebuffers[cur_fb],
                .out.buffer_size    = FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP,
                .out.pic_w          = FRAMEBUFFER_MAX_W,
                .out.pic_h          = FRAMEBUFFER_MAX_H,
                .out.block_offset_x = framebuffer->x,
                .out.block_offset_y = framebuffer->y,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,

                .rotation_angle = ppa_rotation,
                .scale_x        = 1.0,
                .scale_y        = 1.0,
                .rgb_swap       = 0,
                .byte_swap      = 0,
                .mode           = PPA_TRANS_MODE_BLOCKING,
            };

            // ESP_LOGW(TAG, "Compositing %lu x %lu at offset %lu x %lu", framebuffer->framebuffer.w,
            // framebuffer->framebuffer.h, oper_config.out.block_offset_x, oper_config.out.block_offset_y);

            // esp_cache_msync((void*)framebuffer->framebuffer.pixels, 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
            ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config));
            xSemaphoreGive(framebuffer->ready);
            changes = true;

            clock_gettime(CLOCK_MONOTONIC, &end_time);
            long elapsed_ms =
                (end_time.tv_sec - start_time.tv_sec) * 1000L + (end_time.tv_nsec - start_time.tv_nsec) / 1000000L;
            ESP_LOGI(TAG, "FB time: %lu", elapsed_ms);
            sleep_time -= elapsed_ms;
            sleep_time  = sleep_time < 0 ? 0 : sleep_time;

        } else {
            // Timeout, time to update the screen
            if (changes) {
                lcd_device->_draw(lcd_device, 0, 0, FRAMEBUFFER_MAX_W, FRAMEBUFFER_MAX_H, framebuffers[cur_fb]);
                cur_fb  = !cur_fb;
                changes = false;
            }

            sleep_time = target_frame_time_ms;
        }
    }
}

void compositor_init(char const *device) {
    ESP_LOGI(TAG, "Initializing");
    lcd_device = (lcd_device_t *)device_get(device);
    if (!lcd_device) {
        ESP_LOGE(TAG, "Unable to access the LCD device");
        return;
    }

    compositor_queue = xQueueCreate(16, sizeof(managed_framebuffer_t *));
    xTaskCreatePinnedToCore(compositor, "Compositor", 4096, NULL, 20, &compositor_handle, 0);
}
