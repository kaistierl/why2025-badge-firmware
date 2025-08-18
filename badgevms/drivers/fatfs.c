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

#include "driver/sdmmc_host.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "pathfuncs_private.h"
#include "sd_test_io.h"
#include "sdkconfig.h"
#include "sdmmc_cmd.h"

#include <stdbool.h>
#include <stdio.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if CONFIG_SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#define SD_DATA0_GPIO 39
#define SD_DATA1_GPIO 40
#define SD_DATA2_GPIO 41
#define SD_DATA3_GPIO 42
#define SD_SLC_GPIO   43
#define SD_CMD_GPIO   44

#define SDCARD_PIN_CLK SD_SLC_GPIO
#define SDCARD_PIN_CMD SD_CMD_GPIO
#define SDCARD_PIN_D0  SD_DATA0_GPIO
#define SDCARD_PIN_D1  SD_DATA1_GPIO
#define SDCARD_PIN_D2  SD_DATA2_GPIO
#define SDCARD_PIN_D3  SD_DATA3_GPIO

#define SDCARD_PWR_CTRL_LDO_IO_ID       4
#define SDCARD_PWR_CTRL_LDO_INTERNAL_IO 1

#undef SDCARD_SDMMC_SPEED_HS
#undef SDCARD_SDMMC_SPEED_UHS_I_SDR50
#undef SDCARD_SDMMC_SPEED_UHS_I_DDR50

#define IS_UHS1 (SDCARD_SDMMC_SPEED_UHS_I_SDR50 || SDCARD_SDMMC_SPEED_UHS_I_DDR50)

typedef struct {
    filesystem_device_t  filesystem;
    wl_handle_t          wl_handle;
    sdmmc_card_t        *sdmmc_handle;
    sd_pwr_ctrl_handle_t pwr_ctrl_handle;
    char                *base_path;
} fatfs_device_t;

static int fatfs_open(void *dev, path_t *path, int flags, mode_t mode) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);

    int ret = open(unixpath, flags, mode);
    return ret;
}

static int fatfs_close(void *dev, int fd) {
    fsync(fd);
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

static int fatfs_stat(void *dev, path_t *path, struct stat *restrict statbuf) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);
    return stat(unixpath, statbuf);
}

static int fatfs_fstat(void *dev, int fd, struct stat *restrict statbuf) {
    return fstat(fd, statbuf);
}

static int fatfs_unlink(void *dev, path_t *path) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);
    return unlink(unixpath);
}

static int fatfs_rename(void *dev, path_t *oldpath, path_t *newpath) {
    fatfs_device_t *device       = dev;
    char           *old_unixpath = path_to_unix(oldpath);
    char           *new_unixpath = path_to_unix(newpath);
    return rename(old_unixpath, new_unixpath);
}

static int fatfs_mkdir(void *dev, path_t *path, mode_t mode) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);
    return mkdir(unixpath, mode);
}

static int fatfs_rmdir(void *dev, path_t *path) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);
    return rmdir(unixpath);
}

static DIR *fatfs_opendir(void *dev, path_t *path) {
    fatfs_device_t *device   = dev;
    char           *unixpath = path_to_unix(path);
    return opendir(unixpath);
}

static struct dirent *fatfs_readdir(void *dev, DIR *dirp) {
    fatfs_device_t *device = dev;
    return readdir(dirp);
}

static int fatfs_closedir(void *dev, DIR *dirp) {
    fatfs_device_t *device = dev;
    return closedir(dirp);
}

device_t *fatfs_create_spi(char const *devname, char const *partname, bool rw) {
    esp_vfs_fat_mount_config_t const mount_config = {
        .max_files              = 256,
        .format_if_mount_failed = false,
        .allocation_unit_size   = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat            = false,
    };

    fatfs_device_t *dev = malloc(sizeof(fatfs_device_t));
    dev->base_path      = malloc(strlen(devname) + 2);
    dev->base_path[0]   = '/';
    strcpy(dev->base_path + 1, devname);

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(dev->base_path, partname, &mount_config, &dev->wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE("fatfs-spi", "Failed to mount partition");
        goto error;
    }

    // Initialize base device
    device_t *base_dev = &dev->filesystem.device;
    base_dev->type     = DEVICE_TYPE_FILESYSTEM;
    base_dev->_open    = fatfs_open;
    base_dev->_close   = fatfs_close;
    base_dev->_write   = fatfs_write;
    base_dev->_read    = fatfs_read;
    base_dev->_lseek   = fatfs_lseek;

    // Initialize filesystem-specific functions
    filesystem_device_t *fs_dev = &dev->filesystem;
    fs_dev->_stat               = fatfs_stat;
    fs_dev->_fstat              = fatfs_fstat;
    fs_dev->_unlink             = fatfs_unlink;
    fs_dev->_rename             = fatfs_rename;
    fs_dev->_mkdir              = fatfs_mkdir;
    fs_dev->_rmdir              = fatfs_rmdir;
    fs_dev->_opendir            = fatfs_opendir;
    fs_dev->_readdir            = fatfs_readdir;
    fs_dev->_closedir           = fatfs_closedir;

    return (device_t *)dev;

error:
    free(dev->base_path);
    free(dev);
    return NULL;
}

device_t *fatfs_create_sd(char const *devname, bool rw) {
    esp_vfs_fat_mount_config_t const mount_config = {
        .max_files              = 256,
        .format_if_mount_failed = false,
        .allocation_unit_size   = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat            = false,
    };

    fatfs_device_t *dev = malloc(sizeof(fatfs_device_t));
    dev->base_path      = malloc(strlen(devname) + 2);
    dev->base_path[0]   = '/';
    strcpy(dev->base_path + 1, devname);

    // Initialize base device
    device_t *base_dev = &dev->filesystem.device;
    base_dev->type     = DEVICE_TYPE_FILESYSTEM;
    base_dev->_open    = fatfs_open;
    base_dev->_close   = fatfs_close;
    base_dev->_write   = fatfs_write;
    base_dev->_read    = fatfs_read;
    base_dev->_lseek   = fatfs_lseek;

    esp_err_t    err;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot         = SDMMC_HOST_SLOT_0;
#if SDCARD_SDMMC_SPEED_HS
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif SDCARD_SDMMC_SPEED_UHS_I_SDR50
    host.max_freq_khz  = SDMMC_FREQ_SDR50;
    host.flags        &= ~SDMMC_HOST_FLAG_DDR;
#elif SDCARD_SDMMC_SPEED_UHS_I_DDR50
    host.max_freq_khz = SDMMC_FREQ_DDR50;
#endif

#if SDCARD_PWR_CTRL_LDO_INTERNAL_IO
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = SDCARD_PWR_CTRL_LDO_IO_ID,
    };
    err = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &dev->pwr_ctrl_handle);
    if (err != ESP_OK) {
        ESP_LOGE("fatfs-sd", "Failed to create LDO power control driver");
        goto error;
    }

    host.pwr_ctrl_handle = dev->pwr_ctrl_handle;
#endif

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if IS_UHS1
    slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif
    slot_config.gpio_cd = SDMMC_SLOT_NO_CD;
    slot_config.gpio_wp = SDMMC_SLOT_NO_WP;

    // Set bus width to use:
    slot_config.width = 4;
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = SDCARD_PIN_CLK;
    slot_config.cmd = SDCARD_PIN_CMD;
    slot_config.d0  = SDCARD_PIN_D0;
    slot_config.d1  = SDCARD_PIN_D1;
    slot_config.d2  = SDCARD_PIN_D2;
    slot_config.d3  = SDCARD_PIN_D3;
#endif
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    err = esp_vfs_fat_sdmmc_mount(dev->base_path, &host, &slot_config, &mount_config, &dev->sdmmc_handle);
    if (err != ESP_OK) {
#if SDCARD_PWR_CTRL_LDO_INTERNAL_IO
        // Deinitialize the power control driver if it was used
        err = sd_pwr_ctrl_del_on_chip_ldo(host.pwr_ctrl_handle);
        if (err != ESP_OK) {
            ESP_LOGW("fatfs-sd", "Failed to delete the on-chip LDO power control driver");
            goto error;
        }
#endif
        ESP_LOGW("fatfs-sd", "SD card not mounted");
        goto error;
    }

    // Initialize filesystem-specific functions
    filesystem_device_t *fs_dev = &dev->filesystem;
    fs_dev->_stat               = fatfs_stat;
    fs_dev->_fstat              = fatfs_fstat;
    fs_dev->_unlink             = fatfs_unlink;
    fs_dev->_rename             = fatfs_rename;
    fs_dev->_mkdir              = fatfs_mkdir;
    fs_dev->_rmdir              = fatfs_rmdir;
    fs_dev->_opendir            = fatfs_opendir;
    fs_dev->_readdir            = fatfs_readdir;
    fs_dev->_closedir           = fatfs_closedir;

    sdmmc_card_print_info(stdout, dev->sdmmc_handle);

    return (device_t *)dev;
error:
    free(dev->base_path);
    free(dev);
    return NULL;
}
