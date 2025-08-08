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

#include "tca8418.h"

#include "badgevms/event.h"
#include "esp_log.h"
#include "esp_tca8418.h"
#include "freertos/FreeRTOS.h"

#include <sys/time.h>

#define SDA_PIN           18
#define SCL_PIN           20
#define I2C_MASTER_SDA_IO SDA_PIN
#define I2C_MASTER_SCL_IO SCL_PIN

#define TAG "TCA8418"

typedef struct {
    device_t       device;
    key_mod_t      mod_state;
    tca8418_dev_t *keyboard;
} tca8418_device_t;

static keyboard_scancode_t const keymap[80] = {
    KEY_SCANCODE_ESCAPE,    // 0x1
    KEY_SCANCODE_SQUARE,    // 0x2
    KEY_SCANCODE_TRIANGLE,  // 0x3
    KEY_SCANCODE_CROSS,     // 0x4
    KEY_SCANCODE_CIRCLE,    // 0x5
    KEY_SCANCODE_CLOUD,     // 0x6
    KEY_SCANCODE_DIAMOND,   // 0x7
    KEY_SCANCODE_BACKSPACE, // 0x8
    KEY_SCANCODE_0,         // 0x9
    KEY_SCANCODE_MINUS,     // 0xa
    KEY_SCANCODE_GRAVE,     // 0xb
    KEY_SCANCODE_1,         // 0xc
    KEY_SCANCODE_2,         // 0xd
    KEY_SCANCODE_3,         // 0xe
    KEY_SCANCODE_4,         // 0xf

    KEY_SCANCODE_5,   // 0x10
    KEY_SCANCODE_6,   // 0x11
    KEY_SCANCODE_7,   // 0x12
    KEY_SCANCODE_8,   // 0x13
    KEY_SCANCODE_9,   // 0x14
    KEY_SCANCODE_TAB, // 0x15
    KEY_SCANCODE_Q,   // 0x16
    KEY_SCANCODE_W,   // 0x17
    KEY_SCANCODE_E,   // 0x18
    KEY_SCANCODE_R,   // 0x19
    KEY_SCANCODE_T,   // 0x1a
    KEY_SCANCODE_Y,   // 0x1b
    KEY_SCANCODE_U,   // 0x1c
    KEY_SCANCODE_I,   // 0x1d
    KEY_SCANCODE_O,   // 0x1e
    KEY_SCANCODE_FN,  // 0x1f

    KEY_SCANCODE_A,      // 0x20
    KEY_SCANCODE_S,      // 0x21
    KEY_SCANCODE_D,      // 0x22
    KEY_SCANCODE_F,      // 0x23
    KEY_SCANCODE_G,      // 0x24
    KEY_SCANCODE_H,      // 0x25
    KEY_SCANCODE_J,      // 0x26
    KEY_SCANCODE_K,      // 0x27
    KEY_SCANCODE_L,      // 0x28
    KEY_SCANCODE_LSHIFT, // 0x29
    KEY_SCANCODE_Z,      // 0x2a
    KEY_SCANCODE_X,      // 0x2b
    KEY_SCANCODE_C,      // 0x2c
    KEY_SCANCODE_V,      // 0x2d
    KEY_SCANCODE_B,      // 0x2e
    KEY_SCANCODE_N,      // 0x2f

    KEY_SCANCODE_M,          // 0x30
    KEY_SCANCODE_COMMA,      // 0x31
    KEY_SCANCODE_PERIOD,     // 0x32
    KEY_SCANCODE_LEFT,       // 0x33
    KEY_SCANCODE_DOWN,       // 0x34
    KEY_SCANCODE_RIGHT,      // 0x35
    KEY_SCANCODE_SLASH,      // 0x36
    KEY_SCANCODE_UP,         // 0x37
    KEY_SCANCODE_RSHIFT,     // 0x38
    KEY_SCANCODE_SEMICOLON,  // 0x39
    KEY_SCANCODE_APOSTROPHE, // 0x3a
    KEY_SCANCODE_RETURN,     // 0x3b
    KEY_SCANCODE_EQUALS,     // 0x3c
    KEY_SCANCODE_LCTRL,      // 0x3d
    KEY_SCANCODE_LGUI,       // 0x3e
    KEY_SCANCODE_LALT,       // 0x3f

    KEY_SCANCODE_BACKSLASH,   // 0x40
    KEY_SCANCODE_SPACE,       // 0x41
    KEY_SCANCODE_SPACE,       // 0x42
    KEY_SCANCODE_SPACE,       // 0x43
    KEY_SCANCODE_RALT,        // 0x44
    KEY_SCANCODE_P,           // 0x45
    KEY_SCANCODE_LEFTBRACKET, // 0x46
    KEY_SCANCODE_UNKNOWN,     // 0x47
    KEY_SCANCODE_UNKNOWN,     // 0x48
    KEY_SCANCODE_UNKNOWN,     // 0x49
    KEY_SCANCODE_UNKNOWN,     // 0x4a
    KEY_SCANCODE_UNKNOWN,     // 0x4b
    KEY_SCANCODE_UNKNOWN,     // 0x4c
    KEY_SCANCODE_UNKNOWN,     // 0x4d
    KEY_SCANCODE_UNKNOWN,     // 0x4e
    KEY_SCANCODE_UNKNOWN,     // 0x4f

    KEY_SCANCODE_RIGHTBRACKET, // 0x50
};

event_t scancode_to_event(tca8418_device_t *device, uint8_t scancode) {
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint8_t             key = scancode & 0x7F;
    keyboard_scancode_t s   = KEY_SCANCODE_UNKNOWN;

    if (key <= 0x50) {
        s = keymap[key - 1];
    }

    event_t event;
    event.keyboard.down = scancode >> 7; // high bit set means pressed

    key_mod_t mod;
    switch (s) {
        case KEY_SCANCODE_LSHIFT: mod = BADGEVMS_KMOD_LSHIFT; break;
        case KEY_SCANCODE_RSHIFT: mod = BADGEVMS_KMOD_RSHIFT; break;
        case KEY_SCANCODE_LCTRL: mod = BADGEVMS_KMOD_LCTRL; break;
        case KEY_SCANCODE_RCTRL: mod = BADGEVMS_KMOD_RCTRL; break;
        case KEY_SCANCODE_LALT: mod = BADGEVMS_KMOD_LALT; break;
        case KEY_SCANCODE_RALT: mod = BADGEVMS_KMOD_RALT; break;
        case KEY_SCANCODE_LGUI: mod = BADGEVMS_KMOD_LGUI; break;
        default: mod = BADGEVMS_KMOD_NONE;
    }

    if (mod != BADGEVMS_KMOD_NONE) {
        if (event.keyboard.down) {
            device->mod_state |= mod;
        } else {
            device->mod_state &= ~mod;
        }
    }

    if (event.keyboard.down) {
        event.type = EVENT_KEY_DOWN;
    } else {
        event.type = EVENT_KEY_UP;
    }

    event.keyboard.timestamp = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    event.keyboard.scancode  = s;
    event.keyboard.key       = BADGEVMS_SCANCODE_TO_KEYCODE(s);
    event.keyboard.repeat    = false;
    event.keyboard.mod       = device->mod_state;
    event.keyboard.text      = keyboard_get_ascii(s, device->mod_state);

    return event;
}

static int tca8418_open(void *dev, path_t *path, int flags, mode_t mode) {
    return 0;
}

static int tca8418_close(void *dev, int fd) {
    if (fd)
        return -1;
    return 0;
}

static ssize_t tca8418_write(void *dev, int fd, void const *buf, size_t count) {
    return -1;
}

static void tca8418_keyboard_task(void *pvParameters) {
    tca8418_device_t *device = pvParameters;
    // poll keyboard for keypresses
    while (true) {
        while (tca8418_get_event_count(device->keyboard) > 0) {
            sKeyAndChar test = tca8418_get_char(device->keyboard);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    // If ever exits, cleanup
    vTaskDelete(NULL);
}

static ssize_t tca8418_read(void *dev, int fd, void *buf, size_t count) {
    tca8418_device_t *device = dev;
    if (fd)
        return -1;

    size_t written = 0;
    while (written <= count) {
        if (tca8418_get_event_count(device->keyboard)) {
            char    c   = tca8418_get_key(device->keyboard);
            uint8_t key = c & 0x7F;
            if (key > 0x50) {
                ESP_LOGD(TAG, "Illegal scancode 0x%02x, skipping", c);
                continue;
            }
            event_t event = scancode_to_event(dev, c);
            ESP_LOGW(TAG, "Got keyboard event raw 0x%02x scancode 0x%02x", c, event.keyboard.scancode);
            if (written + sizeof(event_t) > count) {
                break;
            }
            memcpy(buf, &event, sizeof(event_t));
            written += sizeof(event_t);
        } else {
            break;
        }
    }

    return written;
}

static ssize_t tca8418_lseek(void *dev, int fd, off_t offset, int whence) {
    return -1;
}

device_t *tca8418_keyboard_create() {
    tca8418_device_t *dev      = calloc(1, sizeof(tca8418_device_t));
    device_t         *base_dev = (device_t *)dev;

    base_dev->type   = DEVICE_TYPE_KEYBOARD;
    base_dev->_open  = tca8418_open;
    base_dev->_close = tca8418_close;
    base_dev->_write = tca8418_write;
    base_dev->_read  = tca8418_read;
    base_dev->_lseek = tca8418_lseek;

    // https://github.com/espressif/esp-iot-solution/discussions/494
    ESP_LOGE(TAG, "Connect to keyboard, this message and the follow error are cosmetic. There is no error");
    dev->keyboard = tca8418_create(
        I2C_MASTER_SCL_IO,
        I2C_MASTER_SDA_IO,
        0,           // Default address
        GPIO_NUM_NC, // Notify pin is not connected
        0,           // Max
        0            // Max
    );

    if (!dev->keyboard) {
        ESP_LOGE(TAG, "keyboard initialization failed");
        free(dev);
        return NULL;
    }

    tca8418_flush(dev->keyboard);

    ESP_LOGE(TAG, "keyboard initialization success");
    // xTaskCreate(tca8418_keyboard_task, "tca8418_keyboard_task", 4096, dev, tskIDLE_PRIORITY, NULL);

    return (device_t *)dev;
}
