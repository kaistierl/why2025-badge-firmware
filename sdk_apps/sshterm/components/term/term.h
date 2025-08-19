#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*term_write_cb)(const uint8_t* data, size_t len, void* user);

bool term_init(int cols, int rows, term_write_cb write_cb, void* user);
void term_shutdown(void);

// Bytes coming *from SSH* into the terminal (to be rendered)
void term_input_bytes(const uint8_t* data, size_t len);

// SDL keyboard → terminal (keysym is SDL_Keycode, mods bitmask; text_utf8 may be NULL)
void term_key_input(int keysym, uint16_t mods, const char* text_utf8);

// Drain terminal’s output buffer (escape sequences to send back to SSH)
void term_flush_output(void);

// Optional (fixed grid in MVP, but keep the API)
void term_resize(int cols, int rows);
