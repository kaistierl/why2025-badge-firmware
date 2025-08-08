#include <math.h>
#include <stdio.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STEP_RATE_IN_MILLISECONDS   50

#define SDL_WINDOW_WIDTH           500
#define SDL_WINDOW_HEIGHT          500

#define BAR_WIDTH                   50
#define BAR_HEIGHT                  15

#define BALL_SIZE                    5
#define BALL_VELOCITY                5

#define ARROW_LEFT  (1 << 0)
#define ARROW_RIGHT (1 << 1)

typedef struct {
    unsigned char arrow_pressed;

    short         pv;

    float         bar_xpos;
    float         ball_xpos;
    float         ball_ypos;
    float         ball_xvel;
    float         ball_yvel;
} GameContext;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    GameContext   game_ctx;
    Uint64        last_step;
} AppState;

static void game_initialize(GameContext *ctx) {
    ctx->pv = 3;

    ctx->bar_xpos = SDL_WINDOW_WIDTH * 1/4;

    ctx->ball_xpos = BALL_SIZE / 2;
    ctx->ball_ypos = SDL_WINDOW_HEIGHT * 3/4 - BALL_SIZE/2;

    ctx->ball_xvel = BALL_VELOCITY * M_SQRT2;
    ctx->ball_yvel = BALL_VELOCITY * M_SQRT2;
}

static void game_step(GameContext *ctx) {
    if (ctx->arrow_pressed & ARROW_LEFT) {
        ctx->bar_xpos -= 10;
        if (ctx->bar_xpos - BAR_WIDTH/2 < 0)
            ctx->bar_xpos = BAR_WIDTH/2;
    }
    if (ctx->arrow_pressed & ARROW_RIGHT) {
        ctx->bar_xpos += 10;
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

        ctx->ball_yvel *= -1;
        ctx->ball_ypos += ctx->ball_yvel;
    }

    // top bounce
    if (ctx->ball_ypos < BALL_SIZE/2) {
        ctx->ball_yvel *= -1;
        ctx->ball_ypos += ctx->ball_yvel;
    }
}

static void game_draw_bar(SDL_Renderer *renderer, GameContext *ctx) {
    SDL_FRect r;
    r.x = ctx->bar_xpos - BAR_WIDTH/2;
    r.y = SDL_WINDOW_HEIGHT - BAR_HEIGHT;
    r.w = BAR_WIDTH;
    r.h = BAR_HEIGHT;
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &r);
}

static void game_draw_ball(SDL_Renderer *renderer, GameContext *ctx) {
    SDL_FRect r;
    r.x = ctx->ball_xpos - BALL_SIZE/2;
    r.y = ctx->ball_ypos - BALL_SIZE/2;
    r.w = BALL_SIZE;
    r.h = BALL_SIZE;
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &r);
}

static void game_draw(SDL_Renderer *renderer, GameContext *ctx) {
    game_draw_bar(renderer, ctx);
    game_draw_ball(renderer, ctx);
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
            game_initialize(ctx);
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

    SDL_SetRenderDrawColor(as->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
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

    game_initialize(&as->game_ctx);
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

