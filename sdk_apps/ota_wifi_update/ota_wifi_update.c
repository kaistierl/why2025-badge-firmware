#define _GNU_SOURCE
#include "badgevms/compositor.h"
#include "badgevms/framebuffer.h"
#include "badgevms/misc_funcs.h"
#include "badgevms/ota.h"
#include "badgevms/process.h"
#include "badgevms/wifi.h"
#include "curl/curl.h"
#include "thirdparty/microui.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

enum State {
    AWAITING_WIFI,
    IDLE,
    CHECKING_VERSION,
    NO_NEW_VERSION_AVAILABLE,
    NEW_VERSION_AVAILABLE,
    UPDATING,
    UPDATE_DONE,
};

typedef struct {
    mu_Context *ctx;
    framebuffer_t *fb;
    window_handle_t window;

    char running_version[32];
    char badgehub_version[32];

    char badgehub_revision[32];

    enum State state;

    bool key_pressed;
    bool updated;
    bool update_available;

    atomic_bool thread_running;

    ota_handle_t ota_session;
    bool ota_error;
} app_state_t;

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

static uint8_t const font_8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // ! (33)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // " (34)
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // # (35)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $ (36)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // % (37)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // & (38)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' (39)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // ( (40)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // ) (41)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // * (42)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // + (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // , (44)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // - (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // . (46)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // / (47)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // 0 (48)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 1 (49)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // 2 (50)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // 3 (51)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // 4 (52)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // 5 (53)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // 6 (54)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // 7 (55)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // 8 (56)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // 9 (57)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // : (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ; (59)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // < (60)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // = (61)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // > (62)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ? (63)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @ (64)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A (65)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B (66)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C (67)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D (68)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // E (69)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // F (70)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G (71)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H (72)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I (73)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J (74)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K (75)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // L (76)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // M (77)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N (78)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O (79)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // P (80)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q (81)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R (82)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S (83)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T (84)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U (85)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V (86)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W (87)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X (88)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y (89)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z (90)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // [ (91)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // \ (92)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // ] (93)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^ (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00}, // _ (95)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // ` (96)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // a (97)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // b (98)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // c (99)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // d (100)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // e (101)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // f (102)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // g (103)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // h (104)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // i (105)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // j (106)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // k (107)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // l (108)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // m (109)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // n (110)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // o (111)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // p (112)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // q (113)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // r (114)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // s (115)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // t (116)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // u (117)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // v (118)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // w (119)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // x (120)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // y (121)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // z (122)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // { (123)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // | (124)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // } (125)
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~ (126)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // DEL (127)
};

static uint8_t const special_symbols[][8] = {
    // □ (square)
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF},
    // △ (triangle)
    {0x08, 0x1C, 0x1C, 0x36, 0x36, 0x63, 0x7F, 0x00},
    // ✕ (cross/X)
    {0x63, 0x36, 0x1C, 0x08, 0x1C, 0x36, 0x63, 0x00},
    // ○ (circle)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},
    // ☁ (cloud)
    {0x00, 0x1C, 0x36, 0x36, 0x7F, 0x00, 0x00, 0x00},
    // ◆ (diamond)
    {0x08, 0x1C, 0x36, 0x63, 0x36, 0x1C, 0x08, 0x00},
    // ← (left arrow)
    {0x00, 0x08, 0x0C, 0x7E, 0x0C, 0x08, 0x00, 0x00},
    // → (right arrow)
    {0x00, 0x10, 0x30, 0x7E, 0x30, 0x10, 0x00, 0x00},
    // ↑ (up arrow)
    {0x08, 0x1C, 0x36, 0x63, 0x08, 0x08, 0x08, 0x00},
    // ↓ (down arrow)
    {0x08, 0x08, 0x08, 0x63, 0x36, 0x1C, 0x08, 0x00},
    // ⟲ (rotate)
    {0x00, 0x1E, 0x06, 0x66, 0x66, 0x60, 0x78, 0x00},
    // ⌫ (backspace)
    {0x00, 0x60, 0x78, 0x7E, 0x78, 0x60, 0x00, 0x00},
    // ↵ (enter/return)
    {0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x00},
    // ⇧ (shift)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x7F, 0x00, 0x00},
};

int get_special_symbol_index(char const *symbol) {
    if (strcmp(symbol, "□") == 0)
        return 0;
    if (strcmp(symbol, "△") == 0)
        return 1;
    if (strcmp(symbol, "✕") == 0)
        return 2;
    if (strcmp(symbol, "○") == 0)
        return 3;
    if (strcmp(symbol, "☁") == 0)
        return 4;
    if (strcmp(symbol, "◆") == 0)
        return 5;
    if (strcmp(symbol, "←") == 0)
        return 6;
    if (strcmp(symbol, "→") == 0)
        return 7;
    if (strcmp(symbol, "↑") == 0)
        return 8;
    if (strcmp(symbol, "↓") == 0)
        return 9;
    if (strcmp(symbol, "⟲") == 0)
        return 10;
    if (strcmp(symbol, "⌫") == 0)
        return 11;
    if (strcmp(symbol, "↵") == 0)
        return 12;
    if (strcmp(symbol, "⇧") == 0)
        return 13;
    return -1;
}

void draw_special_symbol(framebuffer_t *fb, int symbol_index, int x, int y, uint16_t color) {
    if (symbol_index < 0 || symbol_index >= 15)
        return;

    uint8_t const *char_data = special_symbols[symbol_index];

    for (int row = 0; row < 8; row++) {
        uint8_t byte = char_data[row];
        for (int col = 0; col < 8; col++) {
            if (byte & (0x01 << col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb->w && py >= 0 && py < fb->h) {
                    fb->pixels[py * fb->w + px] = color;
                }
            }
        }
    }
}

void draw_rect(framebuffer_t *fb, mu_Rect rect, mu_Color color) {
    uint16_t color565 = rgb888_to_rgb565(color.r, color.g, color.b);

    for (int y = rect.y; y < rect.y + rect.h; y++) {
        for (int x = rect.x; x < rect.x + rect.w; x++) {
            if (x >= 0 && x < fb->w && y >= 0 && y < fb->h) {
                fb->pixels[y * fb->w + x] = color565;
            }
        }
    }
}

void draw_char(framebuffer_t *fb, char c, int x, int y, uint16_t color) {
    if (c < 32 || c > 127)
        return;

    uint8_t const *char_data = font_8x8[c - 32];

    for (int row = 0; row < 8; row++) {
        uint8_t byte = char_data[row];
        for (int col = 0; col < 8; col++) {
            if (byte & (0x01 << col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < fb->w && py >= 0 && py < fb->h) {
                    fb->pixels[py * fb->w + px] = color;
                }
            }
        }
    }
}

void draw_text(framebuffer_t *fb, char const *text, int x, int y, mu_Color color) {
    uint16_t color565 = rgb888_to_rgb565(color.r, color.g, color.b);
    int current_x = x;
    int current_y = y;

    for (int i = 0; text[i];) {
        if (text[i] == '\n') {
            current_x = x;
            current_y += 10;
            i++;
            continue;
        }

        // Check for multi-byte UTF-8 sequences
        if ((unsigned char) text[i] >= 0x80) {
            char symbol[8] = {0};
            int symbol_len = 0;

            while (text[i + symbol_len] && ((unsigned char) text[i + symbol_len] >= 0x80 ||
                                            (symbol_len > 0 && (unsigned char) text[i + symbol_len] >= 0x80))) {
                symbol[symbol_len] = text[i + symbol_len];
                symbol_len++;
                if (symbol_len >= 7)
                    break;
            }

            if (symbol_len == 0 && ((unsigned char) text[i] >= 0x80)) {
                symbol[0] = text[i];
                symbol_len = 1;
            }

            int special_idx = get_special_symbol_index(symbol);
            if (special_idx >= 0) {
                draw_special_symbol(fb, special_idx, current_x, current_y, color565);
            } else {
                // Fallback
                draw_char(fb, '?', current_x, current_y, color565);
            }

            current_x += 8;
            i += symbol_len > 0 ? symbol_len : 1;
        } else {
            // Regular ASCII character
            draw_char(fb, text[i], current_x, current_y, color565);
            current_x += 8;
            i++;
        }
    }
}

int mu_text_width(mu_Font font, char const *text, int len) {
    if (len == -1)
        len = strlen(text);

    int width = 0;
    for (int i = 0; i < len;) {
        // Check for multi-byte UTF-8 sequences
        if ((unsigned char) text[i] >= 0x80) {
            char symbol[8] = {0};
            int symbol_len = 0;

            while (i + symbol_len < len && text[i + symbol_len] &&
                   ((unsigned char) text[i + symbol_len] >= 0x80 ||
                    (symbol_len > 0 && (unsigned char) text[i + symbol_len] >= 0x80))) {
                symbol[symbol_len] = text[i + symbol_len];
                symbol_len++;
                if (symbol_len >= 7)
                    break;
            }

            if (symbol_len == 0 && ((unsigned char) text[i] >= 0x80)) {
                symbol_len = 1;
            }

            width += 8;
            i += symbol_len > 0 ? symbol_len : 1;
        } else {
            width += 8;
            i++;
        }
    }
    return width;
}

int mu_text_height(mu_Font font) {
    return 8;
}

void handle_keyboard_event(app_state_t *app, keyboard_event_t *kbd_event) {
    app->key_pressed = true;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, MemoryStruct *mem) {
    size_t realsize = size * nmemb;

    char *ptr = calloc(realsize + 1, sizeof(char));
    if (!ptr) {
        printf("Not enough memory (calloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(ptr, contents, realsize);
    mem->size = realsize;

    return realsize;
}

static size_t WriteOTACallback(void *contents, size_t size, size_t nmemb, app_state_t *app) {
    bool err;
    size_t realsize = size * nmemb;

    printf("Size: %d \n", realsize);

    char *buffer = (char *) calloc(realsize + 1, sizeof(char));

    memcpy(buffer, contents, realsize);

    err = ota_write(app->ota_session, buffer, realsize);
    if (err == false) {
        app->ota_error = true;
    }

    free(buffer);
    return realsize;
}

void setup_wifi(void *data) {
    app_state_t *app = (app_state_t *) data;


    wifi_connect();
    curl_global_init(0);
    ping_badgehub();

    app->state = IDLE;
    atomic_store(&app->thread_running, false);
    return;
}

void ping_badgehub(void) {
    CURL *curl;
    curl = curl_easy_init();
    if (curl) {
        uint64_t unique_id = get_unique_id();
        char     pingUrl[200];

        snprintf(
            pingUrl,
            sizeof(pingUrl),
            "https://badge.why2025.org/api/v3/ping?id=%08lX%08lX",
            (uint32_t)(unique_id >> 32),
            (uint32_t)unique_id
        );
        printf("Ping URL: %s\n", pingUrl);
        curl_easy_setopt(curl, CURLOPT_URL, pingUrl);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");
        printf("\nDoing Badgehub ping with url: %s\n", pingUrl);
        let retries = 5;
        while (retries && ((res = curl_easy_perform(curl) != CURLE_OK))) {
            printf("\nDoing Badgehub ping with url Retry[%d]: %s\n", 11 - retries, url);
            usleep(500);
            --retries;
        }

        printf("ping_badgehub: result %d", ret);
    } else {
        printf("ping_badgehub: Failed to create curl handle\n");
    }
}

void check_for_update(void *data) {
    app_state_t *app = (app_state_t *) data;
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    char *running = (char *) calloc(32, sizeof(char));
    ota_get_running_version(running);
    strncpy(app->running_version, running, sizeof(app->running_version) - 1);
    printf("Running version: %s \n", running);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(
            curl,
            CURLOPT_URL,
            "https://badge.why2025.org/api/v3/project-latest-revisions/heplaphon_why_firmware_ota_test"
        );
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return;
        }
        printf("BadgeHub firmware revision: %s \n", chunk.memory);
        strncpy(app->badgehub_revision, chunk.memory, sizeof(app->badgehub_revision) - 1);
        char *url = (char *) calloc(256, sizeof(char));
        sprintf(
            url,
            "https://badge.why2025.org/api/v3/projects/heplaphon_why_firmware_ota_test/rev%s/files/version.txt",
            chunk.memory
        );
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return;
        }
        printf("BadgeHub firmware version: %s \n", chunk.memory);
        strncpy(app->badgehub_version, chunk.memory, sizeof(app->badgehub_version) - 1);
        int cmp = strverscmp(running, chunk.memory);
        printf("version compare: %d", cmp);
        if (cmp < 0) {
            app->update_available = true;
            app->state = NEW_VERSION_AVAILABLE;
        } else if (cmp >= 0) {
            app->state = NO_NEW_VERSION_AVAILABLE;
        }
        free(url);
    }
    curl_easy_cleanup(curl);
    free(chunk.memory);
    free(running);
    app->key_pressed = false;
    atomic_store(&app->thread_running, false);
    return;
}

void update_badge(void *data) {
    app_state_t *app = (app_state_t *) data;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    app->ota_error = false;
    app->ota_session = ota_session_open();

    if (curl) {
        char *url = (char *) calloc(256, sizeof(char));
        sprintf(
            url,
            "https://badge.why2025.org/api/v3/projects/heplaphon_why_firmware_ota_test/rev%s/files/badgevms.bin",
            app->badgehub_revision
        );
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteOTACallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)app);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "BadgeVMS-libcurl/1.0");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            app->ota_error = true;
        }
        ota_session_commit(app->ota_session);
        app->updated = true;
        free(url);
    }
    curl_easy_cleanup(curl);
    app->key_pressed = false;
    app->state = UPDATE_DONE;
    atomic_store(&app->thread_running, false);
    return;
}

void ui_update_prompt(app_state_t *app, mu_Context *ctx) {
    // Begin a window just big enough to hold our text
    if (mu_begin_window(ctx, "Updater", mu_rect(210, 0, 300, 200))) {
        mu_layout_row(ctx, 1, (int[]){-1}, 40);
        if (app->state == NEW_VERSION_AVAILABLE) {
            mu_text(ctx, "New version available! \nPress any button to start update");
        } else if (app->state == NO_NEW_VERSION_AVAILABLE) {
            mu_text(ctx, "You are already on the latest version!");
        } else if (app->state == CHECKING_VERSION) {
            mu_text(ctx, "Checking for new versions");
        } else if (app->state == UPDATING) {
            mu_text(ctx, "Updating badge, sit tight!\nThis might take a minute.");
        } else if (app->state == UPDATE_DONE) {
            mu_text(ctx, "Update done! Restart the badge.");
        } else if (app->state == AWAITING_WIFI) {
            mu_text(ctx, "Waiting for a wifi connection");
        } else {
            mu_text(ctx, "Press any button to check for new version");
        }
        if (app->update_available) {
            mu_layout_row(ctx, 1, (int[]){-1}, 40);
            char buffer[100];
            sprintf(buffer, "Current version %s \nNew version: %s", app->running_version, app->badgehub_version);
            mu_text(ctx, buffer);
        }
        if (app->ota_error) {
            mu_layout_row(ctx, 1, (int[]){-1}, 40);
            mu_text(ctx, "There was an error while updating!");
        }
        mu_end_window(ctx);
    }

    if (app->key_pressed && !app->updated && app->update_available == false) {
        app->key_pressed = false;
        if (atomic_load(&app->thread_running) == false) {
            app->state = CHECKING_VERSION;
            pid_t check_thread = thread_create(check_for_update, app, 4096);
            atomic_store(&app->thread_running, true);
        }
    }

    if (app->key_pressed && !app->updated && app->update_available) {
        app->key_pressed = false;
        if (atomic_load(&app->thread_running) == false) {
            app->state = UPDATING;
            pid_t check_thread = thread_create(update_badge, app, 4096);
            atomic_store(&app->thread_running, true);
        }
    }
}

void render_ui(app_state_t *app) {
    mu_Context *ctx = app->ctx;

    uint16_t bg_color = rgb888_to_rgb565(32, 32, 32);
    for (int i = 0; i < app->fb->w * app->fb->h; i++) {
        app->fb->pixels[i] = bg_color;
    }

    mu_begin(ctx);
    ui_update_prompt(app, ctx);
    mu_end(ctx);

    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT:
                draw_text(app->fb, cmd->text.str, cmd->text.pos.x, cmd->text.pos.y, cmd->text.color);
                break;
            case MU_COMMAND_RECT: draw_rect(app->fb, cmd->rect.rect, cmd->rect.color);
                break;
            case MU_COMMAND_ICON:
            case MU_COMMAND_CLIP: break;
        }
    }
}

int main() {
    app_state_t app = {0};

    app.window = window_create("OTA update", (window_size_t) {
        720, 720
    }
    ,
    WINDOW_FLAG_DOUBLE_BUFFERED
    )
    ;
    //int fb_num;
    app.fb = window_framebuffer_create(app.window, (window_size_t) {
        720, 720
    }
    ,
    BADGEVMS_PIXELFORMAT_RGB565
    )
    ;

    app.ctx = malloc(sizeof(mu_Context));
    mu_init(app.ctx);
    app.ctx->text_width = mu_text_width;
    app.ctx->text_height = mu_text_height;

    app.state = AWAITING_WIFI;
    atomic_store(&app.thread_running, false);

    bool running = true;
    long const target_frame_time_us = 16667;

    memset(app.fb->pixels, 0x55, 720 * 720 * 2);

    render_ui(&app);
    window_present(app.window, true, NULL, 0);

    pid_t check_thread = thread_create(setup_wifi, &app, 4096);
    atomic_store(&app.thread_running, true);

    while (running) {
        struct timespec start_time, cur_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        while (1) {
            clock_gettime(CLOCK_MONOTONIC, &cur_time);
            long elapsed_us =
                    (cur_time.tv_sec - start_time.tv_sec) * 1000000L + (cur_time.tv_nsec - start_time.tv_nsec) / 1000L;
            long sleep_time = target_frame_time_us - elapsed_us;

            if (sleep_time <= 0) {
                break;
            }

            event_t event = window_event_poll(app.window, false, sleep_time / 1000);

            switch (event.type) {
                case EVENT_QUIT: running = false;
                    break;
                case EVENT_KEY_DOWN:
                case EVENT_KEY_UP: handle_keyboard_event(&app, &event.keyboard);
                    break;
                default: break;
            }

            render_ui(&app);
        }
        static uint64_t frame_counter = 0;
        frame_counter++;

        window_present(app.window, true, NULL, 0);
    }

    window_destroy(app.window);
    free(app.ctx);

    return 0;
}
