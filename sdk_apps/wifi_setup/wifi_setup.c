// App: GUI WiFi selector with basic text rendering and password input

#include <badgevms/wifi.h>
#include <badgevms/compositor.h>
#include <badgevms/framebuffer.h>
#include <badgevms/event.h>
#include <badgevms/keyboard.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// Reuse the pixel font from the launcher app for text rendering
#include "../badgevms_launcher/font.h"

#define SCREEN_W 720
#define SCREEN_H 720
#define MAX_LIST 100

static inline uint16_t rgb888_to_rgb565_color(uint32_t rgb888) {
    uint8_t r = (rgb888 >> 16) & 0xFF;
    uint8_t g = (rgb888 >> 8) & 0xFF;
    uint8_t b = rgb888 & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static void clear_fb(framebuffer_t *fb, uint32_t rgb888) {
    uint16_t rgb565 = rgb888_to_rgb565_color(rgb888);
    for (uint32_t i = 0, n = fb->w * fb->h; i < n; ++i) {
        fb->pixels[i] = rgb565;
    }
}

static void draw_char(framebuffer_t *fb, int x, int y, char c, uint32_t color) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR)
        return;

    int             char_index = c - FONT_FIRST_CHAR;
    uint16_t const *char_data  = pixel_font[char_index];
    uint16_t        rgb565     = rgb888_to_rgb565_color(color);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint16_t row_data = char_data[row];
        int      py       = y + row;
        if (py < 0 || py >= (int)fb->h)
            continue;
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (row_data & (0x800 >> col)) {
                int px = x + col;
                if (px >= 0 && px < (int)fb->w) {
                    fb->pixels[py * fb->w + px] = rgb565;
                }
            }
        }
    }
}

static void draw_text(framebuffer_t *fb, int x, int y, char const *text, uint32_t color) {
    int current_x = x;
    while (*text) {
        draw_char(fb, current_x, y, *text, color);
        current_x += FONT_WIDTH;
        text++;
    }
}

static void draw_text_bold(framebuffer_t *fb, int x, int y, char const *text, uint32_t color) {
    draw_text(fb, x, y, text, color);
    draw_text(fb, x + 1, y, text, color);
}

static void draw_rect(framebuffer_t *fb, int x, int y, int w, int h, uint32_t rgb888) {
    if (w <= 0 || h <= 0) return;
    uint16_t rgb565 = rgb888_to_rgb565_color(rgb888);
    int      x2     = x + w;
    int      y2     = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (int)fb->w) x2 = fb->w;
    if (y2 > (int)fb->h) y2 = fb->h;
    for (int py = y; py < y2; ++py) {
        uint16_t *row = &fb->pixels[py * fb->w + x];
        for (int px = x; px < x2; ++px) {
            *row++ = rgb565;
        }
    }
}

static void draw_3d_border(framebuffer_t *fb, int x, int y, int w, int h, bool inset) {
    uint32_t light = inset ? 0x636363 : 0xFFFFFF;
    uint32_t dark  = inset ? 0xFFFFFF : 0x636363;
    draw_rect(fb, x, y, w, 2, light);
    draw_rect(fb, x, y, 2, h, light);
    draw_rect(fb, x, y + h - 2, w, 2, dark);
    draw_rect(fb, x + w - 2, y, 2, h, dark);
}

typedef enum { Mode_Select, Mode_Password, Mode_Result } ui_mode_t;

static void load_scan(char ssids[MAX_LIST][33], wifi_auth_mode_t auths[MAX_LIST], int rssis[MAX_LIST], int *num) {
    // Trigger and read cached scan results
    (void)wifi_scan_get_num_results();
    int total = wifi_scan_get_num_results();
    if (total < 0) total = 0;

    // Deduplicate by SSID: keep entry with strongest RSSI and mark as secured if any AP is secured
    int unique = 0;
    for (int i = 0; i < total; ++i) {
        wifi_station_handle s = wifi_scan_get_result(i);
        if (!s) continue;
        char const *ssid = wifi_station_get_ssid(s);
        if (!ssid || !ssid[0]) { wifi_scan_free_station(s); continue; }
        int              rssi = wifi_station_get_rssi(s);
        wifi_auth_mode_t am   = wifi_station_get_authmode(s);

        // find existing
        int found = -1;
        for (int j = 0; j < unique; ++j) {
            if (strncmp(ssids[j], ssid, 33) == 0) { found = j; break; }
        }
        if (found == -1) {
            if (unique < MAX_LIST) {
                strncpy(ssids[unique], ssid, 32);
                ssids[unique][32] = '\0';
                auths[unique]     = am;
                rssis[unique]     = rssi;
                unique++;
            }
        } else {
            // Prefer strongest RSSI
            if (rssi > rssis[found]) rssis[found] = rssi;
            // If any BSSID advertises security, mark secured
            if (am != WIFI_AUTH_OPEN && am != WIFI_AUTH_NONE) {
                auths[found] = am;
            }
        }
        wifi_scan_free_station(s);
    }

    *num = unique;
}

static void sort_by_rssi(char ssids[MAX_LIST][33], wifi_auth_mode_t auths[MAX_LIST], int rssis[MAX_LIST], int num) {
    // Simple insertion sort, since list is small
    for (int i = 1; i < num; ++i) {
        int              key_rssi = rssis[i];
        wifi_auth_mode_t key_auth = auths[i];
        char             key_ssid[33];
        strncpy(key_ssid, ssids[i], sizeof(key_ssid));
        int j = i - 1;
        while (j >= 0 && rssis[j] < key_rssi) {
            rssis[j + 1] = rssis[j];
            auths[j + 1] = auths[j];
            strncpy(ssids[j + 1], ssids[j], 33);
            j--;
        }
        rssis[j + 1] = key_rssi;
        auths[j + 1] = key_auth;
        strncpy(ssids[j + 1], key_ssid, 33);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    window_handle_t win = window_create(
        "WiFi Setup",
        (window_size_t){SCREEN_W, SCREEN_H},
        WINDOW_FLAG_DOUBLE_BUFFERED | WINDOW_FLAG_FULLSCREEN | WINDOW_FLAG_LOW_PRIORITY
    );
    if (!win) return 1;

    framebuffer_t *fb = window_framebuffer_create(win, (window_size_t){SCREEN_W, SCREEN_H}, BADGEVMS_PIXELFORMAT_RGB565);
    if (!fb) return 1;

    char               ssids[MAX_LIST][33] = {{0}};
    wifi_auth_mode_t   auths[MAX_LIST]     = {0};
    int                rssis[MAX_LIST]     = {0};
    int                num                 = 0;
    int                cursor              = 0;
    ui_mode_t          mode                = Mode_Select;
    char               pwd[65]             = {0};
    int                pwd_len             = 0;
    wifi_connection_status_t last_result   = WIFI_ERROR;

    // Initial scan with a couple of retries
    for (int attempt = 0; attempt < 3 && num == 0; ++attempt) {
        load_scan(ssids, auths, rssis, &num);
        if (num > 1) sort_by_rssi(ssids, auths, rssis, num);
    }

    while (1) {
        // Draw UI
        clear_fb(fb, 0x9CA0A0);
        // Window
        draw_rect(fb, 30, 30, SCREEN_W - 60, SCREEN_H - 60, 0xAEB2B2);
        draw_3d_border(fb, 30, 30, SCREEN_W - 60, SCREEN_H - 60, false);
        // Title
        draw_rect(fb, 33, 33, SCREEN_W - 66, 45, 0x808080);
    char title[64];
    snprintf(title, sizeof(title), "WiFi Setup  (%d networks)", num);
    draw_text_bold(fb, 45, 45, title, 0xFFFFFF);
        // Footer
        draw_rect(fb, 33, SCREEN_H - 42, SCREEN_W - 66, 39, 0xD4D0C8);
        draw_3d_border(fb, 33, SCREEN_H - 42, SCREEN_W - 66, 39, true);
        draw_text(
            fb,
            45,
            SCREEN_H - 35,
            mode == Mode_Select ? "UP/DOWN: Navigate   ENTER: Connect   R: Rescan   ESC: Exit"
                                : "Type password  ENTER: Connect  BACKSPACE: Delete  ESC: Cancel",
            0x000000
        );

        int list_x = 48;
        int list_y = 96;
        int list_w = SCREEN_W - 96;
        int list_h = SCREEN_H - 96 - 60;
        draw_rect(fb, list_x, list_y, list_w, list_h, 0xFFFFFF);
        draw_3d_border(fb, list_x, list_y, list_w, list_h, true);

        if (mode == Mode_Select) {
        if (num == 0) {
                draw_text(fb, list_x + 12, list_y + 12, "No networks found. Press R to rescan.", 0x000000);
            } else {
                int item_h = 28;
                int visible = (list_h - 8) / item_h;
                if (visible < 1) visible = 1;
                int start = 0;
                if (cursor >= visible) start = cursor - visible + 1;
                int end = start + visible;
                if (end > num) end = num;
                for (int i = start; i < end; ++i) {
                    int y = list_y + 6 + (i - start) * item_h;
                    if (i == cursor) {
                        draw_rect(fb, list_x + 4, y - 2, list_w - 8, item_h - 2, 0x0078D4);
                    }
            char line[96];
            // Show RSSI in dBm; mark lock if secured
            snprintf(line, sizeof(line), "%s%s  %ddBm",
                 ssids[i][0] ? ssids[i] : "<unknown>",
                 (auths[i] == WIFI_AUTH_OPEN || auths[i] == WIFI_AUTH_NONE) ? "" : "  [LOCK]",
                 rssis[i]);
                    draw_text(
                        fb,
                        list_x + 10,
                        y,
                        line,
                        (i == cursor) ? 0xFFFFFF : 0x000000
                    );
                }
            }
        } else if (mode == Mode_Password) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt), "Password for %s:", ssids[cursor]);
            draw_text(fb, list_x + 12, list_y + 12, prompt, 0x000000);
            // Masked password
            char masked[70];
            int  ml = pwd_len;
            if (ml > 64) ml = 64;
            for (int i = 0; i < ml; ++i) masked[i] = '*';
            masked[ml] = '\0';
            draw_text(fb, list_x + 12, list_y + 12 + 32, masked, 0x000000);
        } else if (mode == Mode_Result) {
            draw_text(
                fb,
                list_x + 12,
                list_y + 12,
                (last_result == WIFI_CONNECTED) ? "Connected!" : "Connection failed.",
                (last_result == WIFI_CONNECTED) ? 0x006400 : 0x8B0000
            );
        }

        window_present(win, true, NULL, 0);

        // Input
        event_t ev = window_event_poll(win, true, 0);
        if (ev.type == EVENT_QUIT) break;
        if (ev.type == EVENT_KEY_DOWN) {
            keyboard_scancode_t sc = ev.keyboard.scancode;
            if (mode == Mode_Select) {
                switch (sc) {
                    case KEY_SCANCODE_ESCAPE: return 0;
                    case KEY_SCANCODE_UP: if (cursor > 0) cursor--; break;
                    case KEY_SCANCODE_DOWN: if (cursor + 1 < num) cursor++; break;
                    case KEY_SCANCODE_R: {
                        load_scan(ssids, auths, rssis, &num);
                        if (num > 1) sort_by_rssi(ssids, auths, rssis, num);
                        if (cursor >= num) cursor = (num > 0) ? num - 1 : 0;
                    } break;
                    case KEY_SCANCODE_RETURN:
                    case KEY_SCANCODE_KP_ENTER: {
                        if (num <= 0) break;
                        if (auths[cursor] == WIFI_AUTH_OPEN) {
                            last_result = wifi_connect_to(ssids[cursor], "");
                            mode        = Mode_Result;
                        } else {
                            pwd_len = 0; pwd[0] = '\0';
                            mode = Mode_Password;
                        }
                    } break;
                    default: break;
                }
            } else if (mode == Mode_Password) {
                switch (sc) {
                    case KEY_SCANCODE_ESCAPE: mode = Mode_Select; break;
                    case KEY_SCANCODE_BACKSPACE:
                        if (pwd_len > 0) { pwd[--pwd_len] = '\0'; }
                        break;
                    case KEY_SCANCODE_RETURN:
                    case KEY_SCANCODE_KP_ENTER: {
                        last_result = wifi_connect_to(ssids[cursor], pwd);
                        mode        = Mode_Result;
                    } break;
                    default: {
                        // Append printable char from event.text if available
                        char ch = ev.keyboard.text;
                        if (ch >= 32 && ch <= 126 && pwd_len < 64) {
                            pwd[pwd_len++] = ch;
                            pwd[pwd_len]   = '\0';
                        }
                    } break;
                }
            } else if (mode == Mode_Result) {
                // Any key exits
                return 0;
            }
        }
    }

    return 0;
}
