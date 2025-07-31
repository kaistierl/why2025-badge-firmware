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

#include "fatfs.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"

#include <stdbool.h>
#include <stdio.h>

#include <fcntl.h>

typedef struct {
    device_t    device;
    wl_handle_t wl_handle;
    char       *base_path;
} fatfs_device_t;

static int fatfs_open(void *dev, path_t *path, int flags, mode_t mode) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);

    int ret = open(unixpath, flags, mode);
    return ret;
}

static int fatfs_close(void *dev, int fd) {
    return close(fd);
}

static ssize_t fatfs_write(void *dev, int fd, void const *buf, size_t count) {
    return write(fd, buf, count);
}

static ssize_t fatfs_read(void *dev, int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}

static ssize_t fatfs_lseek(void *dev, int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

device_t *fatfs_create_spi(char const *devname, char const *partname, bool rw) {
    esp_vfs_fat_mount_config_t const mount_config = {
        .max_files              = 4,
        .format_if_mount_failed = false,
        .allocation_unit_size   = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat            = false,
    };

    fatfs_device_t *dev = malloc(sizeof(fatfs_device_t));
    dev->base_path      = malloc(strlen(devname) + 2);
    dev->base_path[0]   = '/';
    strcpy(dev->base_path + 1, devname);

    esp_err_t err      = esp_vfs_fat_spiflash_mount_rw_wl(dev->base_path, partname, &mount_config, &dev->wl_handle);
    device_t *base_dev = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_BLOCK;
    base_dev->_open  = fatfs_open;
    base_dev->_close = fatfs_close;
    base_dev->_write = fatfs_write;
    base_dev->_read  = fatfs_read;
    base_dev->_lseek = fatfs_lseek;

    return (device_t *)dev;
}
