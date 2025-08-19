#include <stdio.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

//#include "font_terminus_10x20.h"
//#include "font_terminus_8x16.h"
#include "font_leggie_9x18.h"

/* ---------------- Window + grid config ---------------- */

// compile time fullscreen flag
#if defined(BADGEVMS_FULLSCREEN) && BADGEVMS_FULLSCREEN
    #define WIN_FULLSCREEN 1
#else
    #define WIN_FULLSCREEN 0
#endif

static const int WIN_W = 720;
static const int WIN_H = 720;

static const int SCALE     = 1;
static const int PADDING_X = 0;
static const int PADDING_Y = 9;
#define CELL_W   (FONT_WIDTH  * SCALE)
#define CELL_H   (FONT_HEIGHT * SCALE)
#define COLS     ((WIN_W - PADDING_X) / CELL_W)
#define ROWS     ((WIN_H - PADDING_Y) / CELL_H)


/* --------------- App + terminal state ---------------- */

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
} AppState;

typedef struct {
    AppState base;
    char    *screen;         // ROWS*COLS chars (allocated at init)
    int      cx, cy;         // cursor column/row
    int      cursor_blink_ms;
    int      cursor_on;      // toggled by blink timer
    Uint64   last_blink_ticks;
} State;

/* ---------------- Font rendering ------------------- */

static inline int glyph_index(char c) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) return -1;
    return (int)(c - FONT_FIRST_CHAR);
}

// Draw one glyph from pixel_font at pixel position (x,y)
static void draw_glyph(SDL_Renderer *r, char c, int x, int y, SDL_Color fg) {
    int idx = glyph_index(c);
    if (idx < 0) return;

    SDL_SetRenderDrawColor(r, fg.r, fg.g, fg.b, fg.a);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint16_t bits = pixel_font[idx][row];  // 12 bits used
        for (int col = 0; col < FONT_WIDTH; col++) {
            // bit 11 = leftmost, bit 0 = rightmost
            if ((bits >> (FONT_WIDTH - 1 - col)) & 1) {
                SDL_FRect px = {
                    .x = (float)(x + col * SCALE),
                    .y = (float)(y + row * SCALE),
                    .w = (float)SCALE,
                    .h = (float)SCALE
                };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

// Draw a single cell from the terminal grid at (col,row)
static void draw_cell(SDL_Renderer *r, int col, int row, char c, SDL_Color fg) {
    int x = PADDING_X + col * CELL_W;
    int y = PADDING_Y + row * CELL_H;
    draw_glyph(r, c, x, y, fg);
}

/* ---------------- Terminal core ---------------- */

static void term_clear(State *st) {
    memset(st->screen, ' ', ROWS * COLS);
    st->cx = st->cy = 0;
}

static void term_scroll(State *st) {
    // move rows 1..end up by one line
    memmove(st->screen,
            st->screen + COLS,
            (size_t)(ROWS - 1) * COLS);
    // clear last line
    memset(st->screen + (ROWS - 1) * COLS, ' ', COLS);
}

static void term_putc(State *st, char c) {
    switch (c) {
        case '\r': // carriage return
            st->cx = 0;
            break;

        case '\n': // newline
            st->cx = 0;
            st->cy++;
            if (st->cy >= ROWS) { term_scroll(st); st->cy = ROWS - 1; }
            break;

        case '\b': // backspace
            if (st->cx > 0) {
                st->cx--;
                st->screen[st->cy * COLS + st->cx] = ' ';
            } else if (st->cy > 0) {
                st->cy--;
                st->cx = COLS - 1;
                st->screen[st->cy * COLS + st->cx] = ' ';
            }
            break;

        case '\t': { // tab to next multiple of 8 cols
            int next = ((st->cx / 8) + 1) * 8;
            while (st->cx < next) {
                st->screen[st->cy * COLS + st->cx] = ' ';
                st->cx++;
                if (st->cx >= COLS) { st->cx = 0; st->cy++; }
                if (st->cy >= ROWS) { term_scroll(st); st->cy = ROWS - 1; }
            }
            break;
        }

        default:
            if ((unsigned char)c >= 32 && (unsigned char)c <= 126) { // printable ASCII
                st->screen[st->cy * COLS + st->cx] = c;
                st->cx++;
                if (st->cx >= COLS) { st->cx = 0; st->cy++; }
                if (st->cy >= ROWS) { term_scroll(st); st->cy = ROWS - 1; }
            }
            break;
    }
}

static void term_write(State *st, const char *s) {
    for (int i = 0; s[i]; i++) term_putc(st, s[i]);
}

static void term_draw(State *st, SDL_Renderer *r) {
    SDL_Color fg = {255, 255, 255, 255};

    // draw all cells
    for (int y = 0; y < ROWS; y++) {
        const char *row = st->screen + y * COLS;
        for (int x = 0; x < COLS; x++) {
            char c = row[x];
            draw_cell(r, x, y, c, fg);
        }
    }

    // blinking cursor (thin underline)
    Uint64 now = SDL_GetTicks();
    if (now - st->last_blink_ticks >= (Uint64)st->cursor_blink_ms) {
        st->last_blink_ticks = now;
        st->cursor_on = !st->cursor_on;
    }

    if (st->cursor_on) {
        SDL_SetRenderDrawColor(r, 0, 255, 140, 255); // green-ish
        SDL_FRect cur = {
            .x = (float)(PADDING_X + st->cx * CELL_W),
            .y = (float)(PADDING_Y + st->cy * CELL_H + CELL_H - 2),
            .w = (float)CELL_W,
            .h = 2.0f
        };
        SDL_RenderFillRect(r, &cur);
    }
}

/* ---------------- SDL callbacks ---------------- */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (!SDL_SetAppMetadata("Tiny Terminal", "0.1", "com.example.tinyterm"))
        return SDL_APP_FAILURE;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    State *st = (State *)SDL_calloc(1, sizeof(State));
    if (!st) return SDL_APP_FAILURE;
    *appstate = st;

    Uint32 win_flags = WIN_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0;
    st->base.window = SDL_CreateWindow("Tiny Terminal Demo", WIN_W, WIN_H, win_flags);

    if (!st->base.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    st->base.renderer = SDL_CreateRenderer(st->base.window, NULL);
    if (!st->base.renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Allocate terminal buffer
    st->screen = (char *)SDL_malloc(ROWS * COLS);
    if (!st->screen) return SDL_APP_FAILURE;

    st->cursor_blink_ms  = 500;  // blink every 0.5s
    st->cursor_on        = 1;
    st->last_blink_ticks = SDL_GetTicks();

    term_clear(st);
    term_write(st, "Welcome to cyb3rk4t's tiny terminal core demo.\r\n");
    term_write(st, "Type text. ENTER for newline, BACKSPACE to erase, TAB = 8 columns. ESC to exit.\r\n\n");
    term_write(st, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX - 80 char line - XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n");

    SDL_StartTextInput(st->base.window); // enable SDL_EVENT_TEXT_INPUT for printable chars

    SDL_Log("Grid: %dx%d cells (cell=%dx%d, scale=%d)", COLS, ROWS, CELL_W, CELL_H, SCALE);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    State *st = (State *)appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_TEXT_INPUT: {
            // text is UTF-8; for now accept ASCII bytes 32..126
            const char *t = event->text.text;
            for (int i = 0; t[i]; i++) {
                unsigned char ch = (unsigned char)t[i];
                if (ch >= 32 && ch <= 126)
                    term_putc(st, (char)ch);
            }
            break;
        }

        case SDL_EVENT_KEY_DOWN:
            switch (event->key.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    return SDL_APP_SUCCESS;
                case SDL_SCANCODE_RETURN:
                    term_putc(st, '\r'); term_putc(st, '\n');
                    break;
                case SDL_SCANCODE_BACKSPACE:
                    term_putc(st, '\b');
                    break;
                case SDL_SCANCODE_TAB:
                    term_putc(st, '\t');
                    break;
                // Optional cursor movement with arrows
                case SDL_SCANCODE_LEFT:
                    if (st->cx > 0) st->cx--;
                    break;
                case SDL_SCANCODE_RIGHT:
                    if (st->cx < COLS - 1) st->cx++;
                    break;
                case SDL_SCANCODE_UP:
                    if (st->cy > 0) st->cy--;
                    break;
                case SDL_SCANCODE_DOWN:
                    if (st->cy < ROWS - 1) st->cy++;
                    break;
                default: break;
            }
            break;

        default: break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    State *st = (State *)appstate;
    SDL_Renderer *r = st->base.renderer;

    // Clear background
    SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(r);

    // Draw terminal buffer + cursor
    term_draw(st, r);

    SDL_RenderPresent(r);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    if (!appstate) return;
    State *st = (State *)appstate;

    SDL_StopTextInput(st->base.window);

    if (st->screen)      SDL_free(st->screen);
    if (st->base.renderer) SDL_DestroyRenderer(st->base.renderer);
    if (st->base.window)   SDL_DestroyWindow(st->base.window);
    SDL_free(st);
    SDL_Quit();
}
