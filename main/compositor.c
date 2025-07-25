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
#include "drivers/keyboard.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_private/esp_cache_private.h"
#include "memory.h"
#include "task.h"

#include <stdatomic.h>

#include <sys/time.h>

#define TAG "compositor"

typedef struct managed_framebuffer {
    framebuffer_t       framebuffer;
    allocation_range_t *pages;
    size_t              num_pages;
    uint16_t            x;
    uint16_t            y;
    task_info_t        *task_info;
    atomic_flag         clean;
    SemaphoreHandle_t   ready;

    struct managed_framebuffer *next;
    struct managed_framebuffer *prev;
} managed_framebuffer_t;

static TaskHandle_t  compositor_handle;
static lcd_device_t *lcd_device;
static device_t     *keyboard_device;

static managed_framebuffer_t *window_stack      = NULL;
static SemaphoreHandle_t      window_stack_lock = NULL;
static SemaphoreHandle_t      vsync             = NULL;
static SemaphoreHandle_t      ppa               = NULL;

static bool compositor_initialized = false;
static int  cur_fb                 = 1;
uint16_t   *framebuffers[3];

ppa_srm_rotation_angle_t ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;

#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

#define BORDER_TOP_PX 25 // Title bar height
#define BORDER_PX     2  // Border width

// Font system
#define FONT_WIDTH  5
#define FONT_HEIGHT 7

// Bitmap font data (A-Z, 0-9, space)
static unsigned char const font_data[][7] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // A (65) - index 1
    {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
    // B
    {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0},
    // C
    {0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70},
    // D
    {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0},
    // E
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8},
    // F
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80},
    // G
    {0x70, 0x88, 0x80, 0xB8, 0x88, 0x88, 0x78},
    // H
    {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88},
    // I
    {0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70},
    // J
    {0x38, 0x10, 0x10, 0x10, 0x90, 0x90, 0x60},
    // K
    {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88},
    // L
    {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8},
    // M
    {0x88, 0xD8, 0xA8, 0x88, 0x88, 0x88, 0x88},
    // N
    {0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88},
    // O
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
    // P
    {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80},
    // Q
    {0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68},
    // R
    {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88},
    // S
    {0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70},
    // T
    {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20},
    // U
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},
    // V
    {0x88, 0x88, 0x88, 0x88, 0x50, 0x50, 0x20},
    // W
    {0x88, 0x88, 0x88, 0xA8, 0xA8, 0xD8, 0x88},
    // X
    {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88},
    // Y
    {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20},
    // Z
    {0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8},
    // 0 (48) - index 27
    {0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70},
    // 1
    {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70},
    // 2
    {0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8},
    // 3
    {0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70},
    // 4
    {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10},
    // 5
    {0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70},
    // 6
    {0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70},
    // 7
    {0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40},
    // 8
    {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70},
    // 9
    {0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60}
};

// Color scheme for window decorations
typedef struct {
    uint16_t window_outer_border;

    // Foreground (active) window colors
    uint16_t fg_titlebar_background;       // Title bar background (black)
    uint16_t fg_titlebar_text;             // Title text color (white)
    uint16_t fg_titlebar_text_shadow;      // Title text drop shadow (dark gray)
    uint16_t fg_titlebar_corner_accents;   // L-shaped corner accents (white)
    uint16_t fg_titlebar_horizontal_lines; // Horizontal accent lines (gray)
    uint16_t fg_window_background;         // Window content area (white)
    uint16_t fg_window_inner_border;       // Inner content border (black)
    uint16_t fg_window_outer_boundary;     // Outer boundary for visibility (dark gray)
    uint16_t fg_window_decorative_dark;    // Decorative border inner line (gray)
    uint16_t fg_window_decorative_light;   // Decorative border outer line (white)
    uint16_t fg_window_outer_border;       // Window frame border (white)

    // Background (inactive) window colors
    uint16_t bg_titlebar_background;      // Title bar base color (gray)
    uint16_t bg_titlebar_dither_pattern;  // Dither pattern for faded look (light gray)
    uint16_t bg_titlebar_text;            // Title text color (dark gray)
    uint16_t bg_titlebar_stippled_border; // Stippled border around window (darker gray)
    uint16_t bg_window_background;        // Window content area (light gray)
    uint16_t bg_window_inner_border;      // Inner content border (gray)
    uint16_t bg_window_outer_border;      // Window frame border (light gray)
} window_colors_t;

// Default color scheme
IRAM_ATTR static window_colors_t window_colors = {
    .window_outer_border = RGB565(0, 0, 0), // Black

    // Foreground window colors
    .fg_titlebar_background       = RGB565(0, 0, 0),       // Black
    .fg_titlebar_text             = RGB565(255, 255, 255), // White
    .fg_titlebar_text_shadow      = RGB565(64, 64, 64),    // Dark gray
    .fg_titlebar_corner_accents   = RGB565(255, 255, 255), // White
    .fg_titlebar_horizontal_lines = RGB565(128, 128, 128), // Gray
    .fg_window_background         = RGB565(255, 255, 255), // White
    .fg_window_inner_border       = RGB565(0, 0, 0),       // Black
    .fg_window_outer_boundary     = RGB565(64, 64, 64),    // Dark gray
    .fg_window_decorative_dark    = RGB565(128, 128, 128), // Gray
    .fg_window_decorative_light   = RGB565(255, 255, 255), // White
    .fg_window_outer_border       = RGB565(255, 255, 255), // White

    // Background window colors
    .bg_titlebar_background      = RGB565(160, 160, 160), // Gray
    .bg_titlebar_dither_pattern  = RGB565(200, 200, 200), // Light gray
    .bg_titlebar_text            = RGB565(64, 64, 64),    // Dark gray
    .bg_titlebar_stippled_border = RGB565(96, 96, 96),    // Darker gray
    .bg_window_background        = RGB565(240, 240, 240), // Light gray
    .bg_window_inner_border      = RGB565(128, 128, 128), // Gray
    .bg_window_outer_border      = RGB565(200, 200, 200), // Light gray
};

__attribute__((always_inline)) inline static void
    rotate_coordinates(int x, int y, ppa_srm_rotation_angle_t rotation, int *fb_x, int *fb_y) {
    switch (rotation) {
        case PPA_SRM_ROTATION_ANGLE_0:
            *fb_x = x;
            *fb_y = y;
            break;

        case PPA_SRM_ROTATION_ANGLE_90:
            *fb_x = (FRAMEBUFFER_MAX_H - 1) - y;
            *fb_y = x;
            break;

        case PPA_SRM_ROTATION_ANGLE_180:
            *fb_x = (FRAMEBUFFER_MAX_W - 1) - x;
            *fb_y = (FRAMEBUFFER_MAX_H - 1) - y;
            break;

        case PPA_SRM_ROTATION_ANGLE_270:
            *fb_x = y;
            *fb_y = (FRAMEBUFFER_MAX_W - 1) - x;
            break;
    }
}

IRAM_ATTR static int char_to_font_index(char c) {
    if (c == ' ')
        return 0;
    if (c >= 'A' && c <= 'Z')
        return 1 + (c - 'A');
    if (c >= 'a' && c <= 'z')
        return 1 + (c - 'a'); // Map lowercase to uppercase
    if (c >= '0' && c <= '9')
        return 27 + (c - '0');
    return 0; // Default to space
}

#if 0
IRAM_ATTR static void draw_pixel_rotated(uint16_t *fb, int x, int y, uint16_t color) {
    int fb_x = 0, fb_y = 0;
    rotate_coordinates(x, y, ppa_rotation, &fb_x, &fb_y);
    if (fb_x >= 0 && fb_x < FRAMEBUFFER_MAX_W && fb_y >= 0 && fb_y < FRAMEBUFFER_MAX_H) {
        fb[fb_y * FRAMEBUFFER_MAX_W + fb_x] = color;
    }
}
#endif

IRAM_ATTR static void draw_pixel_rotated(uint16_t *fb, int x, int y, uint16_t color) {
    int fb_x = 0, fb_y = 0;
    rotate_coordinates(x, y, ppa_rotation, &fb_x, &fb_y);

    if (fb_x >= 0 && fb_x < FRAMEBUFFER_MAX_W && fb_y >= 0 && fb_y < FRAMEBUFFER_MAX_H) {
        fb[fb_y * FRAMEBUFFER_MAX_W + fb_x] = color;
    } else {
        ESP_LOGE(TAG, "Out of bounds draw: (%d,%d) -> fb(%d,%d)", x, y, fb_x, fb_y);
    }
}

IRAM_ATTR static void draw_filled_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0)
        return;

    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            draw_pixel_rotated(fb, px, py, color);
        }
    }
}

IRAM_ATTR static void draw_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0)
        return;

    // Top edge
    for (int px = x; px < x + width; px++) {
        draw_pixel_rotated(fb, px, y, color);
    }

    // Bottom edge
    if (height > 1) {
        for (int px = x; px < x + width; px++) {
            draw_pixel_rotated(fb, px, y + height - 1, color);
        }
    }

    // Left and right edges
    if (height > 2) {
        for (int py = y + 1; py < y + height - 1; py++) {
            draw_pixel_rotated(fb, x, py, color);
            if (width > 1) {
                draw_pixel_rotated(fb, x + width - 1, py, color);
            }
        }
    }
}

IRAM_ATTR static void draw_char_rotated(uint16_t *fb, char c, int x, int y, uint16_t color) {
    int font_idx = char_to_font_index(c);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char line = font_data[font_idx][row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (line & (0x80 >> col)) {
                draw_pixel_rotated(fb, x + col, y + row, color);
            }
        }
    }
}

IRAM_ATTR static void draw_text_rotated(uint16_t *fb, char const *text, int x, int y, uint16_t color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_char_rotated(fb, text[i], x + i * (FONT_WIDTH + 1), y, color);
    }
}

__attribute__((always_inline)) static inline void push_framebuffer(managed_framebuffer_t *framebuffer) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    managed_framebuffer_t *head = window_stack;

    if (head) {
        managed_framebuffer_t *tail = head->prev;

        framebuffer->next = head;
        framebuffer->prev = tail;
        head->prev        = framebuffer;
        tail->next        = framebuffer;
    } else {
        framebuffer->prev = framebuffer;
        framebuffer->next = framebuffer;
    }

    window_stack = framebuffer;
    xSemaphoreGive(window_stack_lock);
}

__attribute__((always_inline)) static inline void remove_framebuffer(managed_framebuffer_t *framebuffer) {
    if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to get window list mutex");
        abort();
    }

    xSemaphoreGive(window_stack_lock);
}

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
    atomic_flag_test_and_set(&framebuffer->clean);

    if (fb_count > 3) {
        framebuffer->x = (((fb_count - 3)) * (w / 3) - (2 * BORDER_PX));
        framebuffer->y = (((fb_count - 3)) * (h / 3) - BORDER_TOP_PX);
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

    if (compositor_initialized) {
        // If we aren't initialized yet we're allocating lcd driver framebuffers,
        // they are not windows
        push_framebuffer(framebuffer);
    }

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
    atomic_flag_clear(&managed_framebuffer->clean);

    ESP_LOGV(TAG, "Posting framebuffer %p", framebuffer);

    if (block) {
        ESP_LOGV(TAG, "Suspending myself");
        xSemaphoreTake(managed_framebuffer->ready, portMAX_DELAY);
    }
}

IRAM_ATTR static void draw_window_box(managed_framebuffer_t *framebuffer) {
    uint16_t *fb            = framebuffers[cur_fb];
    bool      is_foreground = (framebuffer == window_stack); // Top window is always foreground

    int x      = framebuffer->x;
    int y      = framebuffer->y;
    int width  = framebuffer->framebuffer.w;
    int height = framebuffer->framebuffer.h;

    int total_width  = width + 2 * BORDER_PX;
    int total_height = height + BORDER_TOP_PX;

    draw_rect_rotated(fb, x, y, total_width, total_height, window_colors.window_outer_border);

    // Draw inner border
    uint16_t inner_border_color =
        is_foreground ? window_colors.fg_window_outer_border : window_colors.bg_window_outer_border;
    draw_rect_rotated(fb, x + 1, y + 1, total_width - 2, total_height - 2, inner_border_color);

    // Draw title bar background
    if (is_foreground) {
        // Foreground window: black title bar
        draw_filled_rect_rotated(fb, x + 1, y + 1, total_width, BORDER_TOP_PX, window_colors.fg_titlebar_background);

        // Top-left corner - simple L-shaped accent
        draw_filled_rect_rotated(fb, x + 3, y + 3, 3, 1, window_colors.fg_titlebar_corner_accents);
        draw_filled_rect_rotated(fb, x + 3, y + 3, 1, 3, window_colors.fg_titlebar_corner_accents);
        // Top-right corner - simple L-shaped accent
        draw_filled_rect_rotated(fb, x + total_width - 6, y + 3, 3, 1, window_colors.fg_titlebar_corner_accents);
        draw_filled_rect_rotated(fb, x + total_width - 4, y + 3, 1, 3, window_colors.fg_titlebar_corner_accents);

        // Horizontal accent lines in title bar
        draw_filled_rect_rotated(fb, x + 7, y + 6, total_width - 14, 1, window_colors.fg_titlebar_horizontal_lines);
        draw_filled_rect_rotated(
            fb,
            x + 7,
            y + BORDER_TOP_PX - 7,
            total_width - 14,
            1,
            window_colors.fg_titlebar_horizontal_lines
        );

    } else {
        // Background window: dithered/stippled title bar
        draw_filled_rect_rotated(
            fb,
            x + 2,
            y + 2,
            total_width - 4,
            BORDER_TOP_PX - 3,
            window_colors.bg_titlebar_background
        );

        // Add dither pattern to title bar to make it look faded/inactive
        for (int dither_y = y + 2; dither_y < y + BORDER_TOP_PX - 1; dither_y++) {
            for (int dither_x = x + 2; dither_x < x + total_width - 2; dither_x++) {
                if ((dither_x + dither_y) % 3 == 0) { // Sparse dither pattern
                    draw_pixel_rotated(fb, dither_x, dither_y, window_colors.bg_titlebar_dither_pattern);
                }
            }
        }

        // Stippled border accent for inactive windows
        for (int dot_x = x; dot_x < x + total_width; dot_x += 4) {
            draw_pixel_rotated(fb, dot_x, y, window_colors.bg_titlebar_stippled_border);
            draw_pixel_rotated(fb, dot_x, y + total_height - 1, window_colors.bg_titlebar_stippled_border);
        }
        for (int dot_y = y; dot_y < y + total_height; dot_y += 4) {
            draw_pixel_rotated(fb, x, dot_y, window_colors.bg_titlebar_stippled_border);
            draw_pixel_rotated(fb, x + total_width - 1, dot_y, window_colors.bg_titlebar_stippled_border);
        }
    }

    // Title text
    char const *title           = is_foreground ? "FOREGROUND" : "BACKGROUND";
    int         text_width      = strlen(title) * (FONT_WIDTH + 1) - 1;
    int         title_bar_width = total_width - 4; // Account for borders
    int         text_x          = x + 2 + (title_bar_width - text_width) / 2;
    int         text_y          = y + (BORDER_TOP_PX - FONT_HEIGHT) / 2; // Center vertically in title bar

    if (is_foreground) {
        // Subtle drop shadow effect
        draw_text_rotated(fb, title, text_x + 1, text_y + 1, window_colors.fg_titlebar_text_shadow);
        draw_text_rotated(fb, title, text_x, text_y, window_colors.fg_titlebar_text);
    } else {
        draw_text_rotated(fb, title, text_x, text_y, window_colors.bg_titlebar_text);
    }

    // Inner content border
    uint16_t border_color = is_foreground ? window_colors.fg_window_inner_border : window_colors.bg_window_inner_border;
    int      content_x    = x + BORDER_PX;
    int      content_y    = y + BORDER_TOP_PX;
    draw_rect_rotated(fb, content_x - 1, content_y - 1, width + 2, height + 2, border_color);
}

IRAM_ATTR static void on_refresh(void *ignored) {
    xSemaphoreGive(vsync);

    //if (compositor_initialized) {
    //}
}

IRAM_ATTR static bool
    ppa_event_callback(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data) {
    xSemaphoreGive(ppa);
    return true;
}

// Fixed compositor function
static void IRAM_ATTR NOINLINE_ATTR compositor(void *ignored) {
    static ppa_client_handle_t ppa_srm_handle = NULL;

    ppa_client_config_t ppa_srm_config = {
        .oper_type             = PPA_OPERATION_SRM,
        .max_pending_trans_num = 2,
    };

    ppa_event_callbacks_t ppa_srm_callbacks = {
        .on_trans_done = ppa_event_callback,
    };

    ppa_register_client(&ppa_srm_config, &ppa_srm_handle);
    ppa_client_register_event_callbacks(ppa_srm_handle, &ppa_srm_callbacks);

    ESP_LOGI(TAG, "framebuffers[0] = %p, framebuffers[1] = %p", framebuffers[0], framebuffers[1]);
    while (1) {
        xSemaphoreTake(vsync, portMAX_DELAY);

#if 0
        keyboard_event_ll_t c;
        ssize_t res = keyboard_device->_read(keyboard_device, 0, &c, sizeof(keyboard_event_ll_t));
        if (res > 0) {
           ESP_LOGE(TAG, "Got keyboard scancode 0x%08x, down: %i", c.scancode, c.down);
        }
#endif

        if (xSemaphoreTake(window_stack_lock, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to get window list mutex");
            abort();
        }

        bool changes = false;

        if (window_stack) {
            managed_framebuffer_t *framebuffer = window_stack->prev; // Start with back window

            do {
                // if (atomic_flag_test_and_set(&framebuffer->clean)) {
                //    goto next;
                // }

                changes = true;

                int content_x = framebuffer->x + BORDER_PX;
                int content_y = framebuffer->y + BORDER_TOP_PX;

                int fb_x = 0, fb_y = 0;
                rotate_coordinates(content_x, content_y, ppa_rotation, &fb_x, &fb_y);

                int final_fb_x = fb_x;
                int final_fb_y = fb_y;

                if (ppa_rotation == PPA_SRM_ROTATION_ANGLE_270) {
                    final_fb_y = fb_y - framebuffer->framebuffer.h + 1;
                } else if (ppa_rotation == PPA_SRM_ROTATION_ANGLE_180) {
                    final_fb_x = fb_x - framebuffer->framebuffer.w + 1;
                    final_fb_y = fb_y - framebuffer->framebuffer.h + 1;
                } else if (ppa_rotation == PPA_SRM_ROTATION_ANGLE_90) {
                    final_fb_x = fb_x - framebuffer->framebuffer.w + 1;
                }

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
                    .out.block_offset_x = final_fb_x,
                    .out.block_offset_y = final_fb_y,
                    .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,

                    .rotation_angle = ppa_rotation,
                    .scale_x        = 1.0,
                    .scale_y        = 1.0,
                    .rgb_swap       = 0,
                    .byte_swap      = 0,
                    .mode           = PPA_TRANS_MODE_BLOCKING,
                };

                ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
                esp_cache_msync(framebuffers[cur_fb], 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
                draw_window_box(framebuffer);
                // esp_cache_msync(framebuffers[cur_fb], 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

                xSemaphoreGive(framebuffer->ready);
                // next:
                framebuffer = framebuffer->prev;
            } while (framebuffer != window_stack->prev);
        }

        xSemaphoreGive(window_stack_lock);

        if (changes) {
            lcd_device->_draw(lcd_device, 0, 0, FRAMEBUFFER_MAX_W, FRAMEBUFFER_MAX_H, framebuffers[cur_fb]);
            cur_fb = (cur_fb + 1) % 3;
        }

    }
}

void compositor_init(char const *lcd_device_name, char const *keyboard_device_name) {
    ESP_LOGI(TAG, "Initializing");

    lcd_device = (lcd_device_t *)device_get(lcd_device_name);
    if (!lcd_device) {
        ESP_LOGE(TAG, "Unable to access the LCD device '%s'", lcd_device_name);
        return;
    }

    keyboard_device = (device_t *)device_get(keyboard_device_name);
    if (!keyboard_device) {
        ESP_LOGE(TAG, "Unable to access the keyboard device '%s'", keyboard_device_name);
        // return;
    }

    lcd_device->_getfb(lcd_device, 0, (void *)&framebuffers[0]);
    lcd_device->_getfb(lcd_device, 1, (void *)&framebuffers[1]);
    lcd_device->_getfb(lcd_device, 2, (void *)&framebuffers[2]);

    memset(framebuffers[0], 0xaa, FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP);
    memset(framebuffers[1], 0xaa, FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP);
    memset(framebuffers[2], 0xaa, FRAMEBUFFER_MAX_W * FRAMEBUFFER_MAX_H * FRAMEBUFFER_BPP);
    esp_cache_msync(framebuffers[0], 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    esp_cache_msync(framebuffers[1], 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    esp_cache_msync(framebuffers[2], 720 * 720 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    cur_fb = 1;

    ESP_LOGW(TAG, "Got framebuffers 0: %p, 1: %p, 2: %p", framebuffers[0], framebuffers[1], framebuffers[2]);
    lcd_device->_set_refresh_cb(lcd_device, NULL, on_refresh);

    window_stack_lock = xSemaphoreCreateMutex();
    vsync             = xSemaphoreCreateBinary();
    ppa               = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(compositor, "Compositor", 4096, NULL, 20, &compositor_handle, 0);
    compositor_initialized = true;
}
