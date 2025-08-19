#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "renderer.h"
#include "font_leggie_9x18.h"  // must provide FONT_WIDTH=9, FONT_HEIGHT=18, pixel_font[][]

// Layout: 9 px top padding so 39*18 = 702 fits in 720
static const int PADDING_X = 0;
static const int PADDING_Y = 9;

// Cursor blink (ms)
static const int CURSOR_BLINK_MS = 500;

// ---- Internal state ----
typedef struct {
    uint32_t cp;  // Unicode codepoint
    render_color_t fg;
    render_color_t bg;
} render_cell_t;

typedef struct {
    SDL_Window*   win;
    SDL_Renderer* ren;
    int win_w, win_h;

    // Screen buffer with color info
    render_cell_t screen[RENDER_ROWS * RENDER_COLS];
    bool dirty;

    // cursor
    int   cx, cy;
    bool  cursor_visible;
    bool  cursor_on;
    uint32_t last_blink_ms;

    render_color_t default_fg;
    render_color_t default_bg;
} RState;

static RState g;

// ---- helpers ----
static inline int idx(int x, int y) {
    if (x < 0 || x >= RENDER_COLS || y < 0 || y >= RENDER_ROWS) {
        return -1;  // Invalid coordinates
    }
    return y * RENDER_COLS + x;
}

static inline int glyph_index(uint32_t cp) {
    return (cp < FONT_FIRST_CHAR || cp > FONT_LAST_CHAR)
           ? -1
           : (int)(cp - FONT_FIRST_CHAR);
}

// Fill a solid rect (ints → floats)
static inline void fill_rect(int x, int y, int w, int h) {
    SDL_FRect r = { (float)x, (float)y, (float)w, (float)h };
    SDL_RenderFillRect(g.ren, &r);
}

// Draw one glyph by batching horizontal runs of set bits
static void draw_glyph_runs(uint32_t cp, int px, int py, uint32_t fg_rgb) {
    int gi = glyph_index(cp);
    if (gi < 0) return; // tofu: leave bg

    Uint8 rr = (fg_rgb >> 16) & 0xFF;
    Uint8 gg = (fg_rgb >> 8)  & 0xFF;
    Uint8 bb =  fg_rgb        & 0xFF;
    SDL_SetRenderDrawColor(g.ren, rr, gg, bb, 255);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint16_t bits = pixel_font[gi][row];

        int col = 0;
        while (col < FONT_WIDTH) {
            // skip zeros
            while (col < FONT_WIDTH && ((bits >> (FONT_WIDTH - 1 - col)) & 1) == 0) col++;
            int start = col;
            // accumulate ones
            while (col < FONT_WIDTH && ((bits >> (FONT_WIDTH - 1 - col)) & 1) == 1) col++;
            int run = col - start;
            if (run > 0) {
                fill_rect(px + start, py + row, run, 1);
            }
        }
    }
}

// ---- Public API ----
bool renderer_init(SDL_Window* window, SDL_Renderer* renderer) {
    memset(&g, 0, sizeof(g));
    g.win = window;
    g.ren = renderer;

    SDL_GetWindowSize(g.win, &g.win_w, &g.win_h);

    // default colors
    g.default_fg.rgb = 0xFFFFFF;
    g.default_bg.rgb = 0x000000;

    // Initialize screen with blank cells
    render_cell_t blank = { .cp = ' ', .fg = g.default_fg, .bg = g.default_bg };
    for (int i = 0; i < RENDER_ROWS * RENDER_COLS; i++) {
        g.screen[i] = blank;
    }
    g.dirty = true;

    // cursor
    g.cx = g.cy = 0;
    g.cursor_visible = true;
    g.cursor_on = true;
    g.last_blink_ms = SDL_GetTicks();

    return true;
}

void renderer_shutdown(void) {
    memset(&g, 0, sizeof(g));
}

void renderer_set_cell(int x, int y, uint32_t cp, render_color_t fg, render_color_t bg) {
    if ((unsigned)x >= RENDER_COLS || (unsigned)y >= RENDER_ROWS) return;
    
    int i = idx(x, y);
    if (i < 0) return;
    
    g.screen[i].cp = (cp >= 32 && cp <= 126) ? cp : ' '; // printable ASCII or space
    g.screen[i].fg = fg;
    g.screen[i].bg = bg;
    g.dirty = true;
}

void renderer_scroll_up(int top, int bottom, int lines) {
    if (top < 0) top = 0;
    if (bottom >= RENDER_ROWS) bottom = RENDER_ROWS - 1;
    if (lines <= 0 || top > bottom) return;

    int width = RENDER_COLS;
    int rows  = bottom - top + 1;
    if (lines > rows) lines = rows;

    // Move screen content up
    memmove(&g.screen[idx(0, top)],
            &g.screen[idx(0, top + lines)],
            (size_t)(width * (rows - lines) * sizeof(render_cell_t)));

    // Clear the bottom lines
    render_cell_t blank = { .cp = ' ', .fg = g.default_fg, .bg = g.default_bg };
    for (int y = bottom - lines + 1; y <= bottom; y++) {
        for (int x = 0; x < width; x++) {
            int i = idx(x, y);
            if (i >= 0) g.screen[i] = blank;
        }
    }
    g.dirty = true;
}

void renderer_set_cursor(int x, int y, bool visible) {
    bool changed = false;
    if ((unsigned)x < RENDER_COLS && g.cx != x) {
        g.cx = x;
        changed = true;
    }
    if ((unsigned)y < RENDER_ROWS && g.cy != y) {
        g.cy = y;
        changed = true;
    }
    if (g.cursor_visible != visible) {
        g.cursor_visible = visible;
        changed = true;
    }
    if (changed) g.dirty = true;
}

void renderer_present_if_dirty(uint32_t now_ms) {
    // Check if cursor needs to blink
    bool cursor_changed = false;
    if (now_ms - g.last_blink_ms >= (uint32_t)CURSOR_BLINK_MS) {
        g.last_blink_ms = now_ms;
        g.cursor_on = !g.cursor_on;
        cursor_changed = true;
    }

    // Only redraw if screen is dirty or cursor blinked
    if (!g.dirty && !cursor_changed) return;

    g.dirty = false;

    // 1) Clear entire frame to black
    SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g.ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(g.ren);

    // 2) Draw top + bottom padding
    if (PADDING_Y > 0) {
        fill_rect(0, 0, g.win_w, PADDING_Y);
    }
    const int grid_bottom = PADDING_Y + RENDER_ROWS * RENDER_CELL_H;
    if (grid_bottom < g.win_h) {
        fill_rect(0, grid_bottom, g.win_w, g.win_h - grid_bottom);
    }

    // 3) Draw all glyphs
    for (int y = 0; y < RENDER_ROWS; y++) {
        for (int x = 0; x < RENDER_COLS; x++) {
            int i = idx(x, y);
            if (i < 0) continue;
            
            const render_cell_t* cell = &g.screen[i];
            const int px = PADDING_X + x * RENDER_CELL_W;
            const int py = PADDING_Y + y * RENDER_CELL_H;
            
            // Draw background
            if (cell->bg.rgb != 0x000000) { // If not black background
                uint8_t r = (cell->bg.rgb >> 16) & 0xFF;
                uint8_t g_val = (cell->bg.rgb >> 8) & 0xFF;
                uint8_t b = cell->bg.rgb & 0xFF;
                SDL_SetRenderDrawColor(g.ren, r, g_val, b, SDL_ALPHA_OPAQUE);
                fill_rect(px, py, RENDER_CELL_W, RENDER_CELL_H);
            }
            
            // Draw character
            draw_glyph_runs((uint8_t)cell->cp, px, py, cell->fg.rgb);
        }
    }

    // 4) Underline cursor
    if (g.cursor_visible && g.cursor_on) {
        SDL_SetRenderDrawColor(g.ren, 0, 255, 140, 255);
        fill_rect(PADDING_X + g.cx * RENDER_CELL_W,
                  PADDING_Y + g.cy * RENDER_CELL_H + RENDER_CELL_H - 2,
                  RENDER_CELL_W, 2);
    }

    // 5) Present
    SDL_RenderPresent(g.ren);
}
