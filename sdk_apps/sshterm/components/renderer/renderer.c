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
    render_cell_t screen[TERMINAL_ROWS * TERMINAL_COLS];

    // Per-cell dirty tracking; dirty_count = sum of cell_dirty[]
    uint8_t cell_dirty[TERMINAL_ROWS * TERMINAL_COLS];
    int     dirty_count;

    // Offscreen backbuffer: accumulates all cell draws, blitted to screen on present.
    // Avoids any dependence on GPU swap-chain buffer preservation.
    SDL_Texture* backbuf;

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
    if (x < 0 || x >= TERMINAL_COLS || y < 0 || y >= TERMINAL_ROWS) {
        return -1;
    }
    return y * TERMINAL_COLS + x;
}

static inline int glyph_index(uint32_t cp) {
    return (cp < FONT_FIRST_CHAR || cp > FONT_LAST_CHAR)
           ? -1
           : (int)(cp - FONT_FIRST_CHAR);
}

// Fill a solid rect (ints → floats); draws to whatever SDL render target is active
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

// Mark a cell dirty if not already marked
static inline void mark_dirty(int i) {
    if (i >= 0 && !g.cell_dirty[i]) {
        g.cell_dirty[i] = 1;
        g.dirty_count++;
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
    for (int i = 0; i < TERMINAL_ROWS * TERMINAL_COLS; i++) {
        g.screen[i] = blank;
    }

    // cursor
    g.cx = g.cy = 0;
    g.cursor_visible = true;
    g.cursor_on = true;
    g.last_blink_ms = SDL_GetTicks();

    // Create offscreen backbuffer: all partial updates are drawn here; the backbuf is
    // blitted to the screen on each present, so we are never reliant on the GPU swap
    // chain preserving buffer contents between frames.
    SDL_PixelFormat fmt = SDL_GetWindowPixelFormat(g.win);
    if (fmt == SDL_PIXELFORMAT_UNKNOWN) fmt = SDL_PIXELFORMAT_ARGB8888;
    g.backbuf = SDL_CreateTexture(g.ren, fmt, SDL_TEXTUREACCESS_TARGET,
                                  g.win_w, g.win_h);
    if (!g.backbuf) {
        SDL_Log("renderer_init: failed to create backbuf: %s", SDL_GetError());
        return false;
    }

    // Initialize backbuf to black and draw the static padding strips once
    SDL_SetRenderTarget(g.ren, g.backbuf);
    SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g.ren, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(g.ren);
    if (PADDING_Y > 0) {
        fill_rect(0, 0, g.win_w, PADDING_Y);
    }
    const int grid_bottom = PADDING_Y + TERMINAL_ROWS * RENDER_CELL_H;
    if (grid_bottom < g.win_h) {
        fill_rect(0, grid_bottom, g.win_w, g.win_h - grid_bottom);
    }
    SDL_SetRenderTarget(g.ren, NULL);

    // Mark all cells dirty so the first present draws the full initial screen
    memset(g.cell_dirty, 1, sizeof(g.cell_dirty));
    g.dirty_count = TERMINAL_ROWS * TERMINAL_COLS;

    return true;
}

void renderer_shutdown(void) {
    if (g.backbuf) {
        SDL_DestroyTexture(g.backbuf);
    }
    memset(&g, 0, sizeof(g));
}

void renderer_set_cell(int x, int y, uint32_t cp, render_color_t fg, render_color_t bg) {
    if ((unsigned)x >= TERMINAL_COLS || (unsigned)y >= TERMINAL_ROWS) return;

    int i = idx(x, y);
    if (i < 0) return;

    uint32_t new_cp = (cp >= 32 && cp <= 126) ? cp : ' ';

    // Early-out: skip if content is identical (libvterm can re-damage unchanged cells)
    render_cell_t* cell = &g.screen[i];
    if (cell->cp == new_cp && cell->fg.rgb == fg.rgb && cell->bg.rgb == bg.rgb) return;

    cell->cp = new_cp;
    cell->fg = fg;
    cell->bg = bg;
    mark_dirty(i);
}

void renderer_scroll_up(int top, int bottom, int lines) {
    if (top < 0) top = 0;
    if (bottom >= TERMINAL_ROWS) bottom = TERMINAL_ROWS - 1;
    if (lines <= 0 || top > bottom) return;

    int width = TERMINAL_COLS;
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

    // Mark entire scroll region dirty: all rows shifted, bottom rows blanked
    int start = top * TERMINAL_COLS;
    int count = (bottom - top + 1) * TERMINAL_COLS;
    for (int i = start; i < start + count; i++) {
        if (!g.cell_dirty[i]) { g.cell_dirty[i] = 1; g.dirty_count++; }
    }
}

void renderer_scroll_down(int top, int bottom, int lines) {
    if (top < 0) top = 0;
    if (bottom >= TERMINAL_ROWS) bottom = TERMINAL_ROWS - 1;
    if (lines <= 0 || top > bottom) return;

    int width = TERMINAL_COLS;
    int rows  = bottom - top + 1;
    if (lines > rows) lines = rows;

    // Move screen content down
    memmove(&g.screen[idx(0, top + lines)],
            &g.screen[idx(0, top)],
            (size_t)(width * (rows - lines) * sizeof(render_cell_t)));

    // Clear the top lines
    render_cell_t blank = { .cp = ' ', .fg = g.default_fg, .bg = g.default_bg };
    for (int y = top; y < top + lines; y++) {
        for (int x = 0; x < width; x++) {
            int i = idx(x, y);
            if (i >= 0) g.screen[i] = blank;
        }
    }

    // Mark entire scroll region dirty
    int start = top * TERMINAL_COLS;
    int count = (bottom - top + 1) * TERMINAL_COLS;
    for (int i = start; i < start + count; i++) {
        if (!g.cell_dirty[i]) { g.cell_dirty[i] = 1; g.dirty_count++; }
    }
}

void renderer_set_cursor(int x, int y, bool visible) {
    bool moved = false;
    int old_cx = g.cx, old_cy = g.cy;

    if ((unsigned)x < TERMINAL_COLS && g.cx != x) { g.cx = x; moved = true; }
    if ((unsigned)y < TERMINAL_ROWS && g.cy != y) { g.cy = y; moved = true; }

    bool vis_changed = (g.cursor_visible != visible);
    if (vis_changed) g.cursor_visible = visible;

    if (!moved && !vis_changed) return;

    if (moved) {
        // Old position: redraw without cursor underline
        mark_dirty(idx(old_cx, old_cy));
    }
    // New (or same) position: redraw with updated cursor state
    mark_dirty(idx(g.cx, g.cy));
}

void renderer_present_if_dirty(uint32_t now_ms) {
    // Cursor blink: just mark the cursor cell dirty, no full redraw needed
    if (now_ms - g.last_blink_ms >= (uint32_t)CURSOR_BLINK_MS) {
        g.last_blink_ms = now_ms;
        g.cursor_on = !g.cursor_on;
        mark_dirty(idx(g.cx, g.cy));
    }

    if (g.dirty_count == 0) return;

    // Draw dirty cells into the backbuffer
    SDL_SetRenderTarget(g.ren, g.backbuf);
    SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_NONE);

    for (int y = 0; y < TERMINAL_ROWS; y++) {
        for (int x = 0; x < TERMINAL_COLS; x++) {
            int i = y * TERMINAL_COLS + x;
            if (!g.cell_dirty[i]) continue;

            g.cell_dirty[i] = 0;
            g.dirty_count--;

            const render_cell_t* cell = &g.screen[i];
            const int px = PADDING_X + x * RENDER_CELL_W;
            const int py = PADDING_Y + y * RENDER_CELL_H;

            // Background: always fill (overwrites old glyph pixels; black is explicit, no clear needed)
            uint8_t r  = (cell->bg.rgb >> 16) & 0xFF;
            uint8_t gv = (cell->bg.rgb >> 8)  & 0xFF;
            uint8_t b  =  cell->bg.rgb        & 0xFF;
            SDL_SetRenderDrawColor(g.ren, r, gv, b, SDL_ALPHA_OPAQUE);
            fill_rect(px, py, RENDER_CELL_W, RENDER_CELL_H);

            // Glyph
            draw_glyph_runs(cell->cp, px, py, cell->fg.rgb);

            // Cursor underline drawn as part of this cell's pass
            if (g.cursor_visible && g.cursor_on && x == g.cx && y == g.cy) {
                SDL_SetRenderDrawColor(g.ren, 0, 255, 140, 255);
                fill_rect(px, py + RENDER_CELL_H - 2, RENDER_CELL_W, 2);
            }
        }
    }

    // Blit the full backbuffer to screen and present
    SDL_SetRenderTarget(g.ren, NULL);
    SDL_RenderTexture(g.ren, g.backbuf, NULL, NULL);
    SDL_RenderPresent(g.ren);
}
