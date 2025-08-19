#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

// Fixed grid (Leggie 9x18 on 720x720 with 9px top padding)
enum {
    RENDER_CELL_W = 9,
    RENDER_CELL_H = 18,
    RENDER_COLS   = 80,
    RENDER_ROWS   = 39
};

// Simple RGB color (0xRRGGBB). Alpha not used in MVP.
typedef struct {
    uint32_t rgb;
} render_color_t;

// Initialize with existing SDL window/renderer
bool renderer_init(SDL_Window* window, SDL_Renderer* renderer);
void renderer_shutdown(void);

// Set a single cell (ASCII 32..126 rendered; others become space)
void renderer_set_cell(int x, int y, uint32_t codepoint,
                       render_color_t fg, render_color_t bg);

// Scroll region [top..bottom] (inclusive) up by 'lines'
void renderer_scroll_up(int top, int bottom, int lines);

// Cursor control
void renderer_set_cursor(int x, int y, bool visible);

// Draw: full-frame clear + redraw every call; pass SDL_GetTicks()
void renderer_present_if_dirty(uint32_t now_ms);
