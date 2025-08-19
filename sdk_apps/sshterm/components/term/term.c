#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <vterm.h>
#include "term.h"
#include "../renderer/renderer.h"

// --- Internal state
typedef struct {
    VTerm*       vt;
    VTermScreen* screen;
    term_write_cb write_cb;
    void*        write_user;
    int          cols, rows;
} TState;

static TState g;

// --- Constants

static const uint32_t DEFAULT_FG_COLOR = 0xFFFFFF;  // White
static const uint32_t DEFAULT_BG_COLOR = 0x000000;  // Black

// Bold text brightness enhancement (added to each RGB component)
static const uint32_t BOLD_BRIGHTNESS_DELTA = 0x40;

// Standard 8-color ANSI palette
static const uint32_t ANSI_COLORS[8] = {
    0x000000,  // Black
    0xCD0000,  // Red
    0x00CD00,  // Green
    0xCDCD00,  // Yellow
    0x0000EE,  // Blue
    0xCD00CD,  // Magenta
    0x00CDCD,  // Cyan
    0xE5E5E5   // White
};

// --- Helpers

/**
 * Convert a VTerm color to an RGB value.
 * Handles both RGB and indexed color modes.
 * 
 * @param vterm_color VTerm color structure
 * @param default_color Default color to use if no color is specified
 * @return 24-bit RGB color value (0xRRGGBB)
 */
static uint32_t vterm_color_to_rgb(const VTermColor* vterm_color, uint32_t default_color) {
    if (VTERM_COLOR_IS_RGB(vterm_color)) {
        return (vterm_color->rgb.red << 16) | 
               (vterm_color->rgb.green << 8) | 
               vterm_color->rgb.blue;
    } else if (VTERM_COLOR_IS_INDEXED(vterm_color)) {
        return ANSI_COLORS[vterm_color->indexed.idx & 7];
    }
    return default_color;
}

/**
 * Apply bold enhancement to a color by brightening each RGB component.
 * Prevents overflow by clamping to 0xFF.
 * 
 * @param color Original RGB color (0xRRGGBB)
 * @return Brightened RGB color
 */
static uint32_t apply_bold_brightness(uint32_t color) {
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    
    // Brighten each component safely with overflow protection
    r = (r + BOLD_BRIGHTNESS_DELTA > 0xFF) ? 0xFF : r + BOLD_BRIGHTNESS_DELTA;
    g = (g + BOLD_BRIGHTNESS_DELTA > 0xFF) ? 0xFF : g + BOLD_BRIGHTNESS_DELTA;
    b = (b + BOLD_BRIGHTNESS_DELTA > 0xFF) ? 0xFF : b + BOLD_BRIGHTNESS_DELTA;
    
    return (r << 16) | (g << 8) | b;
}

/**
 * Convert a terminal screen cell to renderer format and update the display.
 * Handles color conversion, bold text enhancement, and UTF-8 character rendering.
 * 
 * @param x Grid column position (0-based)
 * @param y Grid row position (0-based) 
 * @param cell VTerm screen cell containing character and attributes
 */
static void push_to_renderer_from_cell(int x, int y, const VTermScreenCell* cell) {
    // Handle UTF-8 (first character only for now)
    uint32_t cp = cell->chars[0];
    if (cp == 0) cp = ' ';

    // Convert colors using helper functions
    render_color_t fg = { .rgb = vterm_color_to_rgb(&cell->fg, DEFAULT_FG_COLOR) };
    render_color_t bg = { .rgb = vterm_color_to_rgb(&cell->bg, DEFAULT_BG_COLOR) };

    // Apply bold enhancement to foreground color
    if (cell->attrs.bold) {
        fg.rgb = apply_bold_brightness(fg.rgb);
    }

    renderer_set_cell(x, y, cp, fg, bg);
}

/**
 * Repaint a rectangular region of the terminal screen.
 * Queries each cell from libvterm and updates the renderer.
 * 
 * @param start_row Starting row (inclusive)
 * @param end_row Ending row (inclusive)
 * @param start_col Starting column (inclusive)  
 * @param end_col Ending column (inclusive)
 */
static void repaint_rect(int start_row, int end_row, int start_col, int end_col) {
    for (int y = start_row; y <= end_row; y++) {
        for (int x = start_col; x <= end_col; x++) {
            VTermScreenCell cell;
            if (vterm_screen_get_cell(g.screen, (VTermPos){ .row=y, .col=x }, &cell)) {
                push_to_renderer_from_cell(x, y, &cell);
            } else {
                // fallback: blank
                render_color_t fg = { .rgb = 0xFFFFFF };
                render_color_t bg = { .rgb = 0x000000 };
                renderer_set_cell(x, y, ' ', fg, bg);
            }
        }
    }
}

// --- Screen callbacks

static int cb_damage(VTermRect rect, void *user) {
    (void)user;
    // rect start is inclusive; end is exclusive in libvterm
    int sr = rect.start_row;
    int er = rect.end_row - 1;
    int sc = rect.start_col;
    int ec = rect.end_col - 1;
    if (sr <= er && sc <= ec) repaint_rect(sr, er, sc, ec);
    return 1;
}

static int cb_moverect(VTermRect dest, VTermRect src, void *user) {
    (void)user;
    
    // When libvterm scrolls, it typically moves a rectangle of text up or down
    // For scrolling up (most common case), src will be below dest
    if (src.start_row > dest.start_row && 
        dest.start_col == 0 && src.start_col == 0 &&
        dest.end_col == g.cols && src.end_col == g.cols) {
        
        // This is a full-width scroll operation
        int scroll_lines = src.start_row - dest.start_row;
        int top = dest.start_row;
        int bottom = g.rows - 1;  // Use full terminal height
        
        // Validate bounds
        if (top >= 0 && bottom < g.rows && scroll_lines > 0) {
            renderer_scroll_up(top, bottom, scroll_lines);
        }
    }
    // For other move operations, let damage callback handle it
    
    return 1;
}

static int cb_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
    (void)oldpos; (void)user;
    renderer_set_cursor(pos.col, pos.row, visible != 0);
    return 1;
}

static int cb_settermprop(VTermProp prop, VTermValue *val, void *user) {
    (void)user;
    switch (prop) {
        case VTERM_PROP_CURSORVISIBLE: {
            // Get current cursor position first
            VTermPos pos;
            vterm_state_get_cursorpos(vterm_obtain_state(g.vt), &pos);
            renderer_set_cursor(pos.col, pos.row, val->boolean != 0);
            break;
        }
        default:
            break;
    }
    return 1;
}

static int cb_bell(void *user) { (void)user; /* optional: flash/beep */ return 1; }

static int cb_resize(int rows, int cols, void *user) {
    (void)user; (void)rows; (void)cols;
    // MVP: fixed 80x39; ignore
    return 1;
}

static int cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
    (void)cols; (void)cells; (void)user;
    // MVP: no external scrollback
    return 1;
}

static int cb_sb_popline(int cols, VTermScreenCell *cells, void *user) {
    (void)cols; (void)cells; (void)user;
    return 0; // nothing to pop
}

static VTermScreenCallbacks screen_cbs = {
    .damage      = cb_damage,
    .moverect    = cb_moverect,
    .movecursor  = cb_movecursor,
    .settermprop = cb_settermprop,
    .bell        = cb_bell,
    .resize      = cb_resize,
    .sb_pushline = cb_sb_pushline,
    .sb_popline  = cb_sb_popline
};

// --- Output drain (terminal → host bytes)

/**
 * Drain terminal output buffer and send to SSH connection.
 * Called after keyboard input to send escape sequences and responses.
 * Reads all available data from libvterm's output buffer.
 */
static void drain_output(void) {
    if (!g.write_cb) return;
    char outbuf[1024];
    size_t n;
    while ((n = vterm_output_read(g.vt, outbuf, sizeof(outbuf))) > 0) {
        g.write_cb((const uint8_t*)outbuf, n, g.write_user);
    }
}

// --- Public API

bool term_init(int cols, int rows, term_write_cb write_cb, void* user) {
    memset(&g, 0, sizeof(g));
    g.cols = cols;
    g.rows = rows;
    g.write_cb = write_cb;
    g.write_user = user;

    g.vt = vterm_new(rows, cols);
    if (!g.vt) return false;

    vterm_set_utf8(g.vt, 1);

    g.screen = vterm_obtain_screen(g.vt);
    vterm_screen_set_callbacks(g.screen, &screen_cbs, NULL);
    vterm_screen_reset(g.screen, 1 /* hard */);

    // Start with a visible cursor at 0,0
    renderer_set_cursor(0, 0, true);
    return true;
}

void term_shutdown(void) {
    if (g.vt) vterm_free(g.vt);
    memset(&g, 0, sizeof(g));
}

void term_input_bytes(const uint8_t* data, size_t len) {
    if (!g.vt || !data || !len) return;
    vterm_input_write(g.vt, (const char*)data, len);
    // Screen callbacks fire during write → renderer updated
}

void term_key_input(int keysym, uint16_t mods, const char* text_utf8) {
    if (!g.vt) return;

    // Map a minimal set of modifiers (adjust bits to your SDL build if needed)
    VTermModifier vmods = 0;
    if (mods & 0x0001) vmods |= VTERM_MOD_SHIFT; // SDL_KMOD_SHIFT (typical)
    if (mods & 0x0040) vmods |= VTERM_MOD_CTRL;  // SDL_KMOD_CTRL
    if (mods & 0x0100) vmods |= VTERM_MOD_ALT;   // SDL_KMOD_ALT

    switch (keysym) {
        case '\r':      // Return (SDLK_RETURN)
        case '\n':      // Some platforms may deliver LF
            vterm_keyboard_key(g.vt, VTERM_KEY_ENTER, vmods);
            break;
        case '\t':      // Tab (SDLK_TAB)
            vterm_keyboard_key(g.vt, VTERM_KEY_TAB, vmods);
            break;
        case '\x1b':    // Escape (SDLK_ESCAPE)
            vterm_keyboard_key(g.vt, VTERM_KEY_ESCAPE, vmods);
            break;
        case '\b':      // Backspace (SDLK_BACKSPACE)
        case 127:       // DEL often used as backspace by some keyboards
            vterm_keyboard_key(g.vt, VTERM_KEY_BACKSPACE, vmods);
            break;

        // Special keys passed as escape sequences from keyboard.c
        case 0: // Special key indicator - look at text_utf8
            if (text_utf8) {
                // Arrow keys and special sequences
                if (strcmp(text_utf8, "\x1b[A") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_UP, vmods);
                } else if (strcmp(text_utf8, "\x1b[B") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_DOWN, vmods);
                } else if (strcmp(text_utf8, "\x1b[C") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_RIGHT, vmods);
                } else if (strcmp(text_utf8, "\x1b[D") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_LEFT, vmods);
                } else if (strcmp(text_utf8, "\x1b[H") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_HOME, vmods);
                } else if (strcmp(text_utf8, "\x1b[F") == 0) {
                    vterm_keyboard_key(g.vt, VTERM_KEY_END, vmods);
                } else {
                    // Other escape sequences (including Alt combinations), send as is
                    vterm_input_write(g.vt, text_utf8, strlen(text_utf8));
                }
            }
            break;

        default:
            // If we have text (from SDL_EVENT_TEXT_INPUT), send it as UTF-8:
            if (text_utf8 && *text_utf8) {
                // Send UTF-8 bytes directly to libvterm for proper decoding
                vterm_input_write(g.vt, text_utf8, strlen(text_utf8));
            } else if (keysym >= 32 && keysym <= 126) {
                // Printable ASCII from keydown path
                vterm_keyboard_unichar(g.vt, (uint32_t)keysym, vmods);
            } else {
                // Unknown special: ignore in MVP
            }
            break;
    }

    // Send any sequences libvterm generated (to SSH)
    drain_output();
}

void term_flush_output(void) {
    drain_output();
}

void term_resize(int cols, int rows) {
    // Fixed grid in MVP; keep API for future
    (void)cols; (void)rows;
}
