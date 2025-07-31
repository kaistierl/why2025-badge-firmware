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

#include "keyboard.h"

#include <stdint.h>

typedef enum event_type {
    EVENT_NONE,
    EVENT_QUIT,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_WINDOW_RESIZE,
} event_type_t;

// From SDL3
typedef struct {
    uint64_t            timestamp; /**< In nanoseconds, populated using gettimeofday */
    keyboard_scancode_t scancode;  /**< SDL physical key code */
    key_code_t          key;       /**< SDL virtual key code */
    key_mod_t           mod;       /**< current key modifiers */
    char                text;      /**< resolved key including mods */
    bool                down;      /**< true if the key is pressed */
    bool                repeat;    /**< true if this is a key repeat */
} keyboard_event_t;

typedef struct {
    event_type_t type;
    union {
        keyboard_event_t keyboard;
    };
} event_t;
