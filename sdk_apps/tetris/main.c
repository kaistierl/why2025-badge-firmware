#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


SDL_Window   *window = NULL;
SDL_Renderer *renderer = NULL;
uint64_t time_last;

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 500

#define BASE_STEP_TIME 800
#define MIN_STEP_TIME 100

#define KEY_REPEAT_TIME 80

// Field size in blocks
#define FIELD_WIDTH 10
#define FIELD_HEIGHT 20

#define BLOCK_SIZE 20
#define FIELD_OFF_X 50
#define FIELD_OFF_Y 40

const uint8_t piece_colors[] = {
    0, 255, 0, // blue
    255, 0, 0, // red
    0, 0, 255, // green
    0, 255, 255, // cyan
    255, 0, 255, // purple
    255, 255, 255, // white
    255, 255, 0 // yellow
};

const uint8_t piece_rotations[] = {
    1, 2, 2, 2, 4, 4, 4
};
const int8_t piece_data[] = {
    // square block
    0, 0, -1, 0, -1, -1, 0, -1, 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    // line block
    0, 0, -2, 0, -1, 0, 1, 0,
    0, 0, 0, 1, 0, -1, 0, -2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    // S-block
    0, 0, -1, -1, 0, -1, 1, 0, 
    0, 0, 0, 1, 1, 0, 1, -1, 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    // Z-block
    0, 0, -1, 0, 0, -1, 1, -1, 
    0, 0, 1, 1, 1, 0, 0, -1, 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    // L-block
    0, 0, -1, 0, -1, -1, 1, 0, 
    0, 0, 0, 1, 0, -1, 1, -1, 
    0, 0, -1, 0, 1, 0, 1, 1, 
    0, 0, -1, 1, 0, 1, 0, -1, 

    // J-block
    0, 0, -1, 0, 1, 0, 1, -1, 
    0, 0, 0, 1, 0, -1, 1, 1, 
    0, 0, -1, 1, -1, 0, 1, 0, 
    0, 0, 0, 1, 0, -1, -1, -1, 

    // T-block
    0, 0, -1, 0, 0, -1, 1, 0,  
    0, 0, 0, 1, 0, -1, 1, 0, 
    0, 0, -1, 0, 0, 1, 1, 0, 
    0, 0, -1, 0, 0, 1, 0, -1
};

// Tetris state
uint8_t field[FIELD_HEIGHT][FIELD_WIDTH];
int piece_type, piece_rot, piece_x, piece_y;
int score, level, game_over, level_lines_cleared, level_step_time;
int key_pressed_right, key_pressed_left, key_pressed_down, time_last_move;



void tetris_spawn_new_piece() {
    piece_type = rand() % 7;
    piece_rot = 0;
    piece_x = FIELD_WIDTH / 2 - 1;
    piece_y = 0;
}

void tetris_level_up() {
    level++;
    level_lines_cleared = 0;

    if (level_step_time > MIN_STEP_TIME) {
        level_step_time -= 200;
    }
}

void tetris_init() {
    srand(time(NULL));

    score = 0;
    level = 1;
    game_over = 0;
    level_lines_cleared = 0;
    level_step_time = BASE_STEP_TIME;

    // Reset field
    for (int x = 0; x < FIELD_WIDTH; ++x) {
        for (int y = 0; y < FIELD_HEIGHT; ++y) {
            field[y][x] = 255;
        }
    }

    tetris_spawn_new_piece();

}

int tetris_is_collision() {
    for (int square = 0; square < 4; square++) {
        int x_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 0];
        int y_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 1];
        
        int abs_x = piece_x + x_off;
        int abs_y = piece_y + y_off;

        // collision with walls
        if (abs_x < 0 || abs_x >= FIELD_WIDTH) {
            return abs_x < 0 ? 1 : 2;
        }

        // collision with field blocks
        if (field[abs_y][abs_x] != 255) {
            return 3;
        }
    }
    return 0;
}

void tetris_move_left() {
    piece_x--;
    int col = tetris_is_collision();
    if (col != 2 && col > 0) {
        piece_x++;
    }
}


void tetris_move_right() {
    piece_x++;
    int col = tetris_is_collision();
    if (col != 1 && col > 0) {
        piece_x--;
    }
}



void tetris_rotate_piece() {
    piece_rot++;

    if (piece_rot >= piece_rotations[piece_type]) {
        piece_rot = 0;
    }

    // hack: check twice for case of I piece sticking out by 2 blocks
    for (int i = 0; i < 2; ++i) {
        int collision = tetris_is_collision();
        if (collision) {
            if (collision == 1) {
                tetris_move_right();
                printf("left wall collision during rotation\n");
            } else {
                tetris_move_left();
                printf("right wall collision during rotation\n");
            }
        }
    }
}



void tetris_check_for_filled_lines() {
    int lines_cleared = 0;
    for (int y = 0; y < FIELD_HEIGHT; ++y) {
        int fill_count = 0;

        for (int x = 0; x < FIELD_WIDTH; ++x) {
            fill_count += (field[y][x] != 255);
        }
        if (fill_count == 10) {
            lines_cleared++;

            // remove line (everything above goes down one)
            for (int row2 = y; row2 > 0; row2--) {
                for (int x = 0; x < FIELD_WIDTH; ++x) {
                    field[row2][x] = field[row2 - 1][x];
                }
            }
        }
    }

    if (lines_cleared == 1) {
        score += 100 * level;
    } else if (lines_cleared == 2) {
        score += 300 * level;
    } else if (lines_cleared == 3) {
        score += 500 * level;
    } else if (lines_cleared == 4) {
        score += 800 * level;
        // TETRIS! TODO: blink lights?
    }

    level_lines_cleared += lines_cleared;
    if (level_lines_cleared >= level * 10) {
        // level up every level * 10 lines
        tetris_level_up();
    }
}



void tetris_lower_piece() {
    piece_y += 1;

    int collision = 0;

    for (int square = 0; square < 4; ++square) {
        int x_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 0];
        int y_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 1];
        
        int abs_x = piece_x + x_off;
        int abs_y = piece_y + y_off;

        if (abs_y >= 20 || field[abs_y][abs_x] != 255) {
            collision = 1;
            break;
        }
    }

    if (collision) {
        if (piece_y == 1) {
            printf("game over!\n");
            game_over = 1;
            return;
        }

        // add to field
        piece_y -= 1;
        for (int square = 0; square < 4; ++square) {
            int x_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 0];
            int y_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 1];

            int abs_x = piece_x + x_off;
            int abs_y = piece_y + y_off;

            field[abs_y][abs_x] = piece_type;
        }

        // check for line clears
        tetris_check_for_filled_lines();

        // Spawn new piece
        tetris_spawn_new_piece();
    }

}


void tetris_draw_square(int x, int y, int state, int type) {
    if (x < 0 || y < 0 || x >= FIELD_WIDTH || y >= FIELD_HEIGHT) {
        // skip
        return;
    }
    SDL_FRect r;
    r.x = FIELD_OFF_X + BLOCK_SIZE * x;
    r.y = FIELD_OFF_Y + BLOCK_SIZE * y;
    r.w = BLOCK_SIZE;
    r.h = BLOCK_SIZE;

    if (state) {
        // filled, fancy square
        SDL_SetRenderDrawColor(renderer, piece_colors[type*3+0], piece_colors[type*3+1], piece_colors[type*3+2], SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &r);

        SDL_SetRenderDrawColor(renderer, piece_colors[type*3+0]/2, piece_colors[type*3+1]/2, piece_colors[type*3+2]/2, SDL_ALPHA_OPAQUE);
        // r.x += 1;
        // r.y += 1;
        // r.w -= 1;
        // r.h -= 1;
        SDL_RenderRect(renderer, &r);
    } else {
        // Not filled, just black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &r);
    }

}

void tetris_draw_field() {
    for (int x = 0; x < FIELD_WIDTH; ++x) {
        for (int y = 0; y < FIELD_HEIGHT; ++y) {

            tetris_draw_square(x, y, field[y][x] != 255, field[y][x]);
        }
    }
}

void tetris_draw_current_piece() {
    for (int square = 0; square < 4; ++square) {
        int x_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 0];
        int y_off = piece_data[piece_type * 32 + piece_rot * 8 + square * 2 + 1];

        int abs_x = piece_x + x_off;
        int abs_y = piece_y + y_off;

        tetris_draw_square(abs_x, abs_y, 1, piece_type);
    }
}

void tetris_draw_field_outlines() {
    // vertical
    // for (int x = 0; x < FIELD_WIDTH; ++x) {
    //     SDL_RenderDrawLine(x, y, x, y);
    
    // }
    // for (int y = 0; y < FIELD_HEIGHT; ++y) {

    // }

    SDL_SetRenderDrawColor(renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
    
    // Veritcal
    SDL_RenderLine(renderer,
        FIELD_OFF_X,
        FIELD_OFF_Y,
        FIELD_OFF_X,
        FIELD_OFF_Y + FIELD_HEIGHT * BLOCK_SIZE
    );

    SDL_RenderLine(renderer,
        FIELD_OFF_X + FIELD_WIDTH * BLOCK_SIZE,
        FIELD_OFF_Y,
        FIELD_OFF_X + FIELD_WIDTH * BLOCK_SIZE,
        FIELD_OFF_Y + FIELD_HEIGHT * BLOCK_SIZE
    );

    // Horizontal
    SDL_RenderLine(renderer,
        FIELD_OFF_X,
        FIELD_OFF_Y,
        FIELD_OFF_X + FIELD_WIDTH * BLOCK_SIZE,
        FIELD_OFF_Y
    );
    
    SDL_RenderLine(renderer,
        FIELD_OFF_X,
        FIELD_OFF_Y + FIELD_HEIGHT * BLOCK_SIZE,
        FIELD_OFF_X + FIELD_WIDTH * BLOCK_SIZE,
        FIELD_OFF_Y + FIELD_HEIGHT * BLOCK_SIZE
    );
    
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    
    // Create window first
    window = SDL_CreateWindow("tetris", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        printf("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    // Create renderer
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    time_last = SDL_GetTicks();

    tetris_init();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    key_pressed_left = 0;
    key_pressed_right = 0;
    key_pressed_down = 0;
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            switch (event->key.scancode) {
                case SDL_SCANCODE_RIGHT:
                    time_last_move = SDL_GetTicks();
                    key_pressed_right = 1;
                    // tetris_move_right();
                    break;
                case SDL_SCANCODE_LEFT:
                    time_last_move = SDL_GetTicks();
                    key_pressed_left = 1;
                    // tetris_move_left();
                    break;
                case SDL_SCANCODE_UP:
                    tetris_rotate_piece();
                    break;
                case SDL_SCANCODE_DOWN:
                    time_last_move = SDL_GetTicks();
                    key_pressed_down = 1;
                    // tetris_lower_piece();
                    break;
                case SDL_SCANCODE_RETURN:
                    if (game_over) {
                        tetris_init();
                    }
                    break;
            }
        // case SDL_EVENT_KEY_UP:
        //     switch (event->key.scancode) {
        //         case SDL_SCANCODE_RIGHT:
        //             key_pressed_right = 0;
        //             break;
        //         case SDL_SCANCODE_LEFT:
        //             key_pressed_left = 0;
        //             break;
        //     }
        default: break;
    }
    return SDL_APP_CONTINUE;
}

// Draw / game loop
SDL_AppResult SDL_AppIterate(void *appstate) {
    uint64_t now = SDL_GetTicks();

    // Black bg
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);


    char score_str[256];
        sprintf(score_str, "Score: %d", score);
    char level_str[256];
        sprintf(level_str, "Level: %d", level);
    if (game_over) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugText(renderer, WINDOW_WIDTH / 2 - 50, WINDOW_HEIGHT / 2, "GAME OVER");
        SDL_RenderDebugText(renderer, WINDOW_WIDTH / 2 - 50, WINDOW_HEIGHT / 2 + 20, score_str);
        SDL_RenderDebugText(renderer, WINDOW_WIDTH / 2 - 50, WINDOW_HEIGHT / 2 + 40, level_str);
        SDL_RenderDebugText(renderer, WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2 + 60, "(press enter to try again)");
    } else {
            // Move piece
            if (now - time_last_move >= KEY_REPEAT_TIME) {
                if (key_pressed_right) {
                    tetris_move_right();
                }
                else if (key_pressed_left) {
                    tetris_move_left();
                }
                if (key_pressed_down) {
                    tetris_lower_piece();
                }
                time_last_move = now;
            }

            // update
            if (now - time_last >= level_step_time) {
                tetris_lower_piece();
                time_last = now;
            }

            // draw game components
            tetris_draw_field();
            tetris_draw_current_piece();
            tetris_draw_field_outlines();
        
            // score
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
            SDL_RenderDebugText(renderer, FIELD_OFF_X + BLOCK_SIZE * FIELD_WIDTH + BLOCK_SIZE, FIELD_OFF_Y, score_str);
            SDL_RenderDebugText(renderer, FIELD_OFF_X + BLOCK_SIZE * FIELD_WIDTH + BLOCK_SIZE, FIELD_OFF_Y + 20, level_str);

    }
    
    SDL_RenderPresent(renderer); // swap
    return SDL_APP_CONTINUE;

}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}
