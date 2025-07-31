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

#include "compositor_private.h"

#define BORDER_TOP_PX 25 // Title bar height
#define BORDER_PX     2  // Border width

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

void draw_window_box(uint16_t *fb, window_t *window, bool foreground);
