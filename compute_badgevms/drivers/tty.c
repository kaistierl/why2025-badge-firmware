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

#include "tty.h"

#include "freertos/FreeRTOS.h"
#include "rom/uart.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    device_t device;
    bool     is_stdout;
    bool     is_stdin;
} tty_device_t;

static int tty_open(void *dev, path_t *path, int flags, mode_t mode) {
    if (path->directory || path->filename)
        return -1;
    return 0;
}

static int tty_close(void *dev, int fd) {
    if (fd == 0)
        return 0;
    return -1;
}

static ssize_t tty_write(void *dev, int fd, void const *buf, size_t count) {
    tty_device_t *device = dev;
    if (device->is_stdout) {
        for (size_t i = 0; i < count; ++i) {
            putchar(((char const *)buf)[i]);
        }
        return count;
    }
    return 0;
}

static ssize_t tty_read(void *dev, int fd, void *buf, size_t count) {
    tty_device_t *device = dev;

    if (device->is_stdin) {
        uint8_t c;
        while (1) {
            ETS_STATUS s = uart_rx_one_char(&c);
            if (s == ETS_OK)
                break;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        ((char *)buf)[0] = c;
        return 1;
    }
    return 0;
}

static ssize_t tty_lseek(void *dev, int fd, off_t offset, int whence) {
    return (off_t)-1;
}

device_t *tty_create(bool is_stdout, bool is_stdin) {
    tty_device_t *dev      = malloc(sizeof(tty_device_t));
    device_t     *base_dev = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_BLOCK;
    base_dev->_open  = tty_open;
    base_dev->_close = tty_close;
    base_dev->_write = tty_write;
    base_dev->_read  = tty_read;
    base_dev->_lseek = tty_lseek;

    dev->is_stdout = is_stdout;
    dev->is_stdin  = is_stdin;

    return (device_t *)dev;
}
