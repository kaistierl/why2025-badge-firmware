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

#include "pixel_functions.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "font.h"

#define TAG "pixel_functions"

extern rotation_angle_t rotation;

#if 0
IRAM_ATTR void draw_pixel_rotated(uint16_t *fb, int x, int y, uint16_t color) {
    int fb_x = 0, fb_y = 0;
    rotate_coordinates(x, y, rotation, &fb_x, &fb_y);
    if (fb_x >= 0 && fb_x < FRAMEBUFFER_MAX_W && fb_y >= 0 && fb_y < FRAMEBUFFER_MAX_H) {
        fb[fb_y * FRAMEBUFFER_MAX_W + fb_x] = color;
    }
}
#endif

IRAM_ATTR void draw_pixel_rotated(uint16_t *fb, int x, int y, uint16_t color) {
    int fb_x = 0, fb_y = 0;
    rotate_coordinates(x, y, rotation, &fb_x, &fb_y);

    if (fb_x >= 0 && fb_x < FRAMEBUFFER_MAX_W && fb_y >= 0 && fb_y < FRAMEBUFFER_MAX_H) {
        fb[fb_y * FRAMEBUFFER_MAX_W + fb_x] = color;
    } else {
        ESP_LOGV(TAG, "Out of bounds draw: (%d,%d) -> fb(%d,%d)", x, y, fb_x, fb_y);
    }
}

IRAM_ATTR void draw_filled_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0)
        return;

    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            draw_pixel_rotated(fb, px, py, color);
        }
    }
}

IRAM_ATTR void draw_rect_rotated(uint16_t *fb, int x, int y, int width, int height, uint16_t color) {
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

IRAM_ATTR int char_to_font_index(char c) {
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

IRAM_ATTR void draw_char_rotated(uint16_t *fb, char c, int x, int y, uint16_t color) {
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

IRAM_ATTR void draw_text_rotated(uint16_t *fb, char const *text, int x, int y, uint16_t color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_char_rotated(fb, text[i], x + i * (FONT_WIDTH + 1), y, color);
    }
}
