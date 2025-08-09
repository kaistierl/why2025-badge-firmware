#include <math.h>
#include <stdio.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STEP_RATE_IN_MILLISECONDS 50

#define SDL_WINDOW_WIDTH  500
#define SDL_WINDOW_HEIGHT 500

#define ARROW_LEFT  (1 << 0)
#define ARROW_RIGHT (1 << 1)

#define BAR_WIDTH     50
#define BAR_HEIGHT    15
#define BALL_SIZE      5

#define GAME_SPEED_INIT 5
#define GAME_SPEED_INC  0.03

#define BRICKS_NCOL   7
#define BRICKS_NROW  10
#define BRICKS_YMIN  60
#define BRICKS_YMAX 340
#define BRICKS_MGIN   5

#define COLL_LEFT   (1 << 0)
#define COLL_RIGHT  (1 << 1)
#define COLL_TOP    (1 << 2)
#define COLL_BOTTOM (1 << 3)
#define COLL_H      (COLL_LEFT | COLL_RIGHT )
#define COLL_V      (COLL_TOP  | COLL_BOTTOM)

static SDL_FRect brick2rect(unsigned short col, unsigned short row) {
    SDL_FRect r;
    r.w = ( SDL_WINDOW_WIDTH           - (BRICKS_NCOL + 1)*BRICKS_MGIN)/BRICKS_NCOL;
    r.h = ((BRICKS_YMAX - BRICKS_YMIN) - (BRICKS_NROW + 1)*BRICKS_MGIN)/BRICKS_NROW;

    r.x = BRICKS_MGIN + col * (r.w + BRICKS_MGIN);
    r.y = BRICKS_YMIN + row * (r.h + BRICKS_MGIN);

    return r;
}

static unsigned char test_coll(const SDL_FPoint *p, const SDL_FRect *r) {
    // look, it's 2 am and I just had my 8th coffe...

    if (p->x < r->x)
        return 0;
    if (r->x + r->w < p->x)
        return 0;
    if (p->y < r->y)
        return 0;
    if (r->y + r->h < p->y)
        return 0;

    const float relp_x = p->x - r->x;
    const float relp_y = p->y - r->y;
    const float diag_nw_se_y =         relp_x  / r->w * r->h;
    const float diag_sw_ne_y = (r->w - relp_x) / r->w * r->h;

    if (relp_y > diag_nw_se_y) {
        if (relp_y > diag_sw_ne_y)
            return COLL_BOTTOM;
        else
            return COLL_LEFT;
    }
    else {
        if (relp_y > diag_sw_ne_y)
            return COLL_RIGHT;
        else
            printf("coll: top\n");
    }
}

typedef struct {
    unsigned char arrow_pressed;

    short              pv;
    unsigned long long score;

    float speed;

    float bar_xpos;

    float ball_xpos;
    float ball_ypos;
    float ball_xvel;
    float ball_yvel;

    unsigned char bricks_alive[BRICKS_NROW][BRICKS_NCOL];
} GameContext;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    GameContext   game_ctx;
    Uint64        last_step;
} AppState;

static void game_init_ball(GameContext *ctx) {
    ctx->bar_xpos = SDL_WINDOW_WIDTH * 1/4;

    ctx->ball_xpos = BALL_SIZE / 2;
    ctx->ball_ypos = SDL_WINDOW_HEIGHT * 3/4 - BALL_SIZE/2;

    ctx->ball_xvel = ctx->speed * 0.5 * M_SQRT2;
    ctx->ball_yvel = ctx->speed * 0.5 * M_SQRT2;
}

static void game_init(GameContext *ctx) {
    ctx->pv    = 3;
    ctx->score = 0;
    ctx->speed = GAME_SPEED_INIT;

    game_init_ball(ctx);

    for (int col=0; col < BRICKS_NCOL; col++)
        for (int row=0; row < BRICKS_NROW; row++)
            ctx->bricks_alive[row][col] = 1;
}

static void game_hit(GameContext *ctx) {
    ctx->score++;
    ctx->speed += GAME_SPEED_INC;
}

static void game_test_brick(GameContext *ctx, unsigned short col, unsigned short row) {
    if (! ctx->bricks_alive[row][col])
        return;

    SDL_FPoint p     = (SDL_FPoint){.x = ctx->ball_xpos, .y = ctx->ball_ypos};
    SDL_FRect  brick = brick2rect(col, row);
    unsigned char coll = test_coll(&p, &brick);
    if (coll & COLL_V) {
        ctx->ball_yvel *= -1;
        ctx->bricks_alive[row][col] = 0;
        game_hit(ctx);
    }
    else if (coll & COLL_H) {
        ctx->ball_xvel *= -1;
        ctx->bricks_alive[row][col] = 0;
        game_hit(ctx);
    }
}

static void game_step(GameContext *ctx) {
    if (ctx->pv <= 0)
        return;

    if (ctx->arrow_pressed & ARROW_LEFT) {
        ctx->bar_xpos -= ctx->speed * 2;
        if (ctx->bar_xpos - BAR_WIDTH/2 < 0)
            ctx->bar_xpos = BAR_WIDTH/2;
    }
    if (ctx->arrow_pressed & ARROW_RIGHT) {
        ctx->bar_xpos += ctx->speed * 2;
        if (ctx->bar_xpos +  BAR_WIDTH/2 > SDL_WINDOW_WIDTH-1)
            ctx->bar_xpos = -BAR_WIDTH/2 + SDL_WINDOW_WIDTH-1;
    }

    // ball motion
    ctx->ball_xpos += ctx->ball_xvel;
    ctx->ball_ypos += ctx->ball_yvel;

    // side bounces
    if (ctx->ball_xpos < BALL_SIZE/2)
        ctx->ball_xvel *= -1;
    if (ctx->ball_xpos + BALL_SIZE/2 > SDL_WINDOW_WIDTH-1)
        ctx->ball_xvel *= -1;

    // bottom bounce
    if (ctx->ball_ypos - BALL_SIZE/2 > SDL_WINDOW_HEIGHT - BAR_HEIGHT - 1) {
        if (ctx->bar_xpos - BAR_WIDTH/2 >= ctx->ball_xpos || ctx->ball_xpos >= ctx->bar_xpos + BAR_WIDTH/2) {
            ctx->pv--;
            game_init_ball(ctx);
        }
        else
            ctx->ball_yvel *= -1;
    }

    // top bounce
    if (ctx->ball_ypos < BALL_SIZE/2)
        ctx->ball_yvel *= -1;

    // brick collision
    for (int col=0; col < BRICKS_NCOL; col++)
        for (int row=0; row < BRICKS_NROW; row++)
            game_test_brick(ctx, col, row);
}

static void game_draw_gameover(SDL_Renderer *renderer) {
    const char  *text       = "Game Over :3";
    const float  text_width = 60;             // fuck that shit

    SDL_SetRenderDrawColor(renderer, 0xff,0x00,0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, SDL_WINDOW_WIDTH/2 - text_width, SDL_WINDOW_HEIGHT/2, text);
}

static void game_draw_info(SDL_Renderer *renderer, const GameContext *ctx) {
    char info_text[256];
    snprintf(info_text, sizeof(info_text), "PV: %hd SCORE: %llu", ctx->pv, ctx->score);
    info_text[sizeof(info_text)-1] = '\0';

    SDL_SetRenderDrawColor(renderer, 0xff,0xff,0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, 10, 10, info_text);
}

static void game_draw_bar(SDL_Renderer *renderer, const GameContext *ctx) {
    SDL_FRect r;
    r.x = ctx->bar_xpos - BAR_WIDTH/2;
    r.y = SDL_WINDOW_HEIGHT - BAR_HEIGHT;
    r.w = BAR_WIDTH;
    r.h = BAR_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 0xff,0xff,0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &r);
}

static void game_draw_ball(SDL_Renderer *renderer, const GameContext *ctx) {
    SDL_FRect r;
    r.x = ctx->ball_xpos - BALL_SIZE/2;
    r.y = ctx->ball_ypos - BALL_SIZE/2;
    r.w = BALL_SIZE;
    r.h = BALL_SIZE;
    SDL_SetRenderDrawColor(renderer, 0xff,0xff,0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &r);
}

static void game_draw(SDL_Renderer *renderer, const GameContext *ctx) {
    game_draw_info(renderer, ctx);

    if (ctx->pv <= 0) {
        game_draw_gameover(renderer);
        return;
    }

    game_draw_bar(renderer, ctx);
    game_draw_ball(renderer, ctx);

    const SDL_FPoint ball_center = (SDL_FPoint) {
        .x = ctx->ball_xpos,
        .y = ctx->ball_ypos,
    };
#if 0
    for (int col=0; col < BRICKS_NCOL; col++) {
        for (int row=0; row < BRICKS_NROW; row++) {
            SDL_FRect r = brick2rect(col, row);
            if (SDL_PointInRectFloat(&ball_center, &r)) {
                SDL_SetRenderDrawColor(renderer, 0x00,0x00,0xff, SDL_ALPHA_OPAQUE);
                SDL_RenderFillRect(renderer, &r);
            }
            else {
                SDL_SetRenderDrawColor(renderer, 0xff,0xff,0xff, SDL_ALPHA_OPAQUE);
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }
#else
    SDL_SetRenderDrawColor(renderer, 0xff,0xff,0xff, SDL_ALPHA_OPAQUE);
    for (int col=0; col < BRICKS_NCOL; col++) {
        for (int row=0; row < BRICKS_NROW; row++) {
            SDL_FRect r = brick2rect(col, row);
            if (ctx->bricks_alive[row][col])
                SDL_RenderFillRect(renderer, &r);
        }
    }
#endif
}

static SDL_AppResult handle_key_press_event_(GameContext *ctx, SDL_Scancode key_code) {
    switch (key_code) {
        /* Quit. */
        //case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_Q:
            return SDL_APP_SUCCESS;

        /* Restart the game as if the program was launched. */
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_R:
            game_init(ctx);
            break;

        case 0x126: ctx->arrow_pressed |= ARROW_LEFT;  break;
        case 0x127: ctx->arrow_pressed |= ARROW_RIGHT; break;
    }
    return SDL_APP_CONTINUE;
}

static SDL_AppResult handle_key_release_event_(GameContext *ctx, SDL_Scancode key_code) {
    switch (key_code) {
        case 0x126: ctx->arrow_pressed &= ~ARROW_LEFT;  break;
        case 0x127: ctx->arrow_pressed &= ~ARROW_RIGHT; break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState     *as  = (AppState *)appstate;
    GameContext  *ctx = &as->game_ctx;
    Uint64 const  now = SDL_GetTicks();

    while ((now - as->last_step) >= STEP_RATE_IN_MILLISECONDS) {
        game_step(ctx);
        as->last_step += STEP_RATE_IN_MILLISECONDS;
    }

    SDL_SetRenderDrawColor(as->renderer, 0x00,0x00,0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(as->renderer);

    game_draw(as->renderer, ctx);

    SDL_RenderPresent(as->renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    size_t i;

    printf("SDL_AppInit");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as) {
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    // Create window first
    as->window = SDL_CreateWindow("arkanoid", SDL_WINDOW_WIDTH, SDL_WINDOW_HEIGHT, 0);
    if (!as->window) {
        printf("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check display capabilities
    SDL_DisplayID          display      = SDL_GetDisplayForWindow(as->window);
    SDL_DisplayMode const *current_mode = SDL_GetCurrentDisplayMode(display);
    if (current_mode) {
        printf(
            "Current display mode: %dx%d @%.2fHz, format: %s",
            current_mode->w,
            current_mode->h,
            current_mode->refresh_rate,
            SDL_GetPixelFormatName(current_mode->format)
        );
    }

    // Create renderer
    as->renderer = SDL_CreateRenderer(as->window, NULL);
    if (!as->renderer) {
        printf("Failed to create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Check renderer properties
    SDL_PropertiesID props = SDL_GetRendererProperties(as->renderer);
    if (props) {
        char const *name = SDL_GetStringProperty(props, SDL_PROP_RENDERER_NAME_STRING, "Unknown");
        printf("Renderer: %s", name);

        SDL_PixelFormat const *formats =
            (SDL_PixelFormat const *)SDL_GetPointerProperty(props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
        if (formats) {
            printf("Supported texture formats:");
            for (int j = 0; formats[j] != SDL_PIXELFORMAT_UNKNOWN; j++) {
                printf("  Format %d: %s", j, SDL_GetPixelFormatName(formats[j]));
            }
        }
    }

    game_init(&as->game_ctx);
    as->last_step = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    GameContext *ctx = &((AppState *)appstate)->game_ctx;
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            return handle_key_press_event_(ctx, event->key.scancode);
        case SDL_EVENT_KEY_UP:
            return handle_key_release_event_(ctx, event->key.scancode);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (appstate != NULL) {
        AppState *as = (AppState *)appstate;
        SDL_DestroyRenderer(as->renderer);
        SDL_DestroyWindow(as->window);
        SDL_free(as);
    }
}

