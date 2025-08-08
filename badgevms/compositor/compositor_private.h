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

#pragma once

#include "badgevms/compositor.h"
#include "badgevms/framebuffer.h"
#include "badgevms_config.h"
#include "memory.h"
#include "task.h"

#include <stdatomic.h>

#define TOP_BAR_PX  50
#define SIDE_BAR_PX 0

#define MAX_VISIBLE_RECTS 64

typedef struct {
    window_rect_t rects[MAX_VISIBLE_RECTS];
    int           count;
} rect_array_t;

typedef struct {
    window_rect_t rects[4];
    int           count;
} small_rect_array_t;

typedef struct managed_framebuffer {
    framebuffer_t       framebuffer;
    int                 w;
    int                 h;
    pixel_format_t      format;
    allocation_range_t *head_pages;
    allocation_range_t *tail_pages;
    size_t              num_pages;
    atomic_flag         clean;
} managed_framebuffer_t;

typedef struct window {
    managed_framebuffer_t *framebuffers[2];
    uint8_t                front_fb;
    uint8_t                back_fb;
    window_flag_t          flags;
    char                  *title;
    int                    fb_dirty;

    window_rect_t    rect;
    // Store the previous rect if we go fullscreen/maximized
    window_rect_t    rect_orig;
    rect_array_t     visible;
    atomic_uintptr_t task_info;
    QueueHandle_t    event_queue;

    struct window *next;
    struct window *prev;
} window_t;

bool compositor_init(char const *lcd_device_name, char const *keyboard_device_name);
void window_destroy_task(window_handle_t window);
