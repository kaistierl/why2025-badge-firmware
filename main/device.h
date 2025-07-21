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

#include "drivers/keyboard.h"
#include "khash.h"
#include "pathfuncs.h"

#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>

KHASH_MAP_INIT_STR(devtable, void *);

typedef enum {
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_LCD,
    DEVICE_TYPE_KEYBOARD,
} device_type_t;

typedef struct {
    device_type_t type;
    int (*_open)(void *dev, path_t *path, int flags, mode_t mode);
    int (*_close)(void *dev, int fd);
    ssize_t (*_write)(void *dev, int fd, void const *buf, size_t count);
    ssize_t (*_read)(void *dev, int fd, void *buf, size_t count);
    ssize_t (*_lseek)(void *dev, int fd, off_t offset, int whence);
} device_t;

typedef struct lcd_device_s {
    device_t device;
    void (*_draw)(void *dev, int x, int y, int w, int h, void *pixels);
    void (*_getfb)(void *dev, int num, void **pixels);
    void (*_set_refresh_cb)(void *dev, void *user_data, void (*callback)(void *user_data));
} lcd_device_t;

typedef struct keyboard_device_s {
    device_t device;

} keyboard_device_t;

void      device_init();
int       device_register(char const *name, device_t *device);
device_t *device_get(char const *name);
