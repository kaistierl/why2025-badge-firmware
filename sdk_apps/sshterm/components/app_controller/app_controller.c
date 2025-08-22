#include "app_controller.h"
#include "../ssh_manager/ssh_manager.h"
#include "../term/term.h"
#include "../input_system/input_system.h"
#include "../renderer/renderer.h"
#include "../keyboard/keyboard.h"
#include "../test_mode/test_mode.h"
#include "../ui_manager/ui_manager.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Application controller implementation structure
struct app_controller {
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool system_initialized;
    app_state_t* current_app_state;  // Application state
    bool shutdown_requested;         // For graceful shutdown
};

// Forward declarations
static void term_write_callback(const uint8_t* data, size_t len, void* user);
static bool app_controller_initialize_system(app_controller_t* controller);
static void app_controller_cleanup_system(app_controller_t* controller);
static bool app_controller_run_main_loop(app_controller_t* controller, app_state_t* app_state);
static bool app_controller_handle_sdl_event(app_controller_t* controller, app_state_t* app_state, const SDL_Event* event, bool* should_quit);
static void app_controller_route_prompt_char(app_controller_t* controller, app_state_t* app_state, char ch);

// Application controller lifecycle
bool app_controller_init(app_controller_t** controller) {
    if (!controller) {
        return false;
    }
    
    *controller = calloc(1, sizeof(app_controller_t));
    if (!*controller) {
        return false;
    }
    
    // Initialize shutdown state
    (*controller)->shutdown_requested = false;
    
    return true;
}

void app_controller_shutdown(app_controller_t* controller) {
    if (!controller) {
        return;
    }
    
    if (controller->system_initialized) {
        app_controller_cleanup_system(controller);
    }
    
    free(controller);
}

void app_controller_cleanup(app_controller_t* controller, app_state_t* app_state) {
    if (!controller || !app_state) {
        return;
    }
    
    // Clean up SSH connections and state
    ssh_manager_cleanup(app_state);
}

// Main application lifecycle
bool app_controller_run(app_controller_t* controller) {
    if (!controller) {
        return false;
    }
    
    // Create application state
    app_state_t app_state = app_controller_create_default_state();
    controller->current_app_state = &app_state;
    
    // Initialize the system (SDL, renderer, etc.)
    if (!app_controller_initialize_system(controller)) {
        fprintf(stderr, "Failed to initialize system\n");
        return false;
    }
    
    // Run the main application loop
    bool success = app_controller_run_main_loop(controller, &app_state);
    
    // Cleanup system resources
    app_controller_cleanup_system(controller);
    
    return success;
}

void app_controller_request_shutdown(app_controller_t* controller) {
    if (controller) {
        controller->shutdown_requested = true;
        
        // Also cleanup SSH if connected
        if (controller->current_app_state) {
            ssh_manager_cleanup(controller->current_app_state);
        }
    }
}

app_state_t app_controller_create_default_state(void) {
    app_state_t state = {
        .ssh_connected = false,
        .ssh_connecting = false,
        .had_ssh_session = false,
        .input_mode = INPUT_MODE_STARTUP_CHOICE,
        .connection_input = {0}
    };
    return state;
}

void app_controller_set_app_state(app_controller_t* controller, app_state_t* app_state) {
    if (controller) {
        controller->current_app_state = app_state;
    }
}

// System management
bool app_controller_initialize_system(app_controller_t* controller) {
    if (!controller) {
        return false;
    }
    
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    // Create window
    const int WIN_W = 720;
    const int WIN_H = 720;
    
#if defined(BADGEVMS_FULLSCREEN) && BADGEVMS_FULLSCREEN
    Uint32 win_flags = SDL_WINDOW_FULLSCREEN;
#else
    Uint32 win_flags = 0;
#endif
    
    controller->window = SDL_CreateWindow("SSH Terminal", WIN_W, WIN_H, win_flags);
    if (!controller->window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    controller->renderer = SDL_CreateRenderer(controller->window, NULL);
    if (!controller->renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(controller->window);
        SDL_Quit();
        return false;
    }
    
    // Initialize renderer
    if (!renderer_init(controller->window, controller->renderer)) {
        fprintf(stderr, "Renderer initialization failed\n");
        SDL_DestroyRenderer(controller->renderer);
        SDL_DestroyWindow(controller->window);
        SDL_Quit();
        return false;
    }

    // Enable SDL text input events
    SDL_StartTextInput(controller->window);
    
    controller->system_initialized = true;
    return true;
}

bool app_controller_run_main_loop(app_controller_t* controller, app_state_t* app_state) {
    if (!controller || !app_state) {
        return false;
    }
    
    // Initialize terminal emulator
    if (!term_init(RENDER_COLS, RENDER_ROWS, term_write_callback, app_state)) {
        fprintf(stderr, "term_init failed\n");
        return false;
    }

    // Display startup mode selection prompt
    app_controller_return_to_startup(app_state);

    bool running = true;
    SDL_Event ev;
    
    while (running && !controller->shutdown_requested) {
        // Process all pending events without blocking
        while (SDL_PollEvent(&ev)) {
            bool should_quit = false;
            if (!app_controller_handle_sdl_event(controller, app_state, &ev, &should_quit)) {
                // Error handling event - continue or break based on severity
                continue;
            }
            if (should_quit) {
                running = false;
                break;
            }
        }

        // Check for shutdown request after event handling
        if (controller->shutdown_requested) {
            break;
        }

        // Poll SSH connection for incoming data
        ssh_manager_poll_and_read(app_state);
        
        // Update screen if needed (handles cursor blinking internally)
        renderer_present_if_dirty(SDL_GetTicks());
        
        // Small delay to prevent excessive CPU usage while maintaining responsiveness
        SDL_Delay(16);  // ~60 FPS refresh rate
    }
    
    return true;
}

void app_controller_cleanup_system(app_controller_t* controller) {
    if (!controller || !controller->system_initialized) {
        return;
    }
    
    term_shutdown();
    renderer_shutdown();
    SDL_StopTextInput(controller->window);
    SDL_DestroyRenderer(controller->renderer);
    SDL_DestroyWindow(controller->window);
    SDL_Quit();
    
    controller->system_initialized = false;
}

// Event handling
bool app_controller_handle_sdl_event(app_controller_t* controller, 
                                    app_state_t* app_state, 
                                    const SDL_Event* event, 
                                    bool* should_quit) {
    if (!controller || !app_state || !event || !should_quit) {
        return false;
    }
    
    switch (event->type) {
        case SDL_EVENT_QUIT:
            *should_quit = true;
            break;

        case SDL_EVENT_KEY_DOWN: {
            // Always check for Ctrl+Q first (global quit) - works in all modes
            if ((event->key.mod & SDL_KMOD_CTRL) && event->key.key == SDLK_Q) {
                *should_quit = true;
                break;
            }

            // Handle Escape key context-sensitively
            if (event->key.key == SDLK_ESCAPE) {
                if (app_state->input_mode == INPUT_MODE_NORMAL) {
                    // In normal mode: only handle ESC if not connected to SSH
                    if (!ssh_manager_is_connected(app_state)) {
                        // Not connected - return to startup menu
                        app_controller_return_to_startup(app_state);
                        break;
                    }
                    // Connected to SSH - fall through to pass ESC to terminal
                } else {
                    // In prompt modes - handle ESC to cancel/go back
                    input_system_handle_escape_key(app_state);
                    break;
                }
            }

            // In prompt modes, handle special keys here and do not forward to VTerm
            if (app_state->input_mode != INPUT_MODE_NORMAL) {
                switch (event->key.key) {
                    case SDLK_BACKSPACE:
                        app_controller_route_prompt_char(controller, app_state, '\b');
                        break;
                    case SDLK_DELETE:
                        // Treat Delete like backspace for simple line editing in prompts
                        app_controller_route_prompt_char(controller, app_state, 127);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        app_controller_route_prompt_char(controller, app_state, '\r');
                        break;
                    // Ignore arrows and other navigation keys in prompts
                    case SDLK_UP: case SDLK_DOWN: case SDLK_LEFT: case SDLK_RIGHT:
                    case SDLK_HOME: case SDLK_END: case SDLK_PAGEUP: case SDLK_PAGEDOWN:
                    case SDLK_INSERT: case SDLK_TAB:
                        break;
                    default:
                        // Let text input (SDL_EVENT_TEXT_INPUT) feed printable chars
                        break;
                }
                break;
            }

            // Normal terminal operation: use existing keyboard path
            handle_key_event(&event->key, should_quit);
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            if (event->text.text && *event->text.text) {
                // Handle startup choice and SSH input mode characters
                if (app_state->input_mode != INPUT_MODE_NORMAL) {
                    // Send each character to appropriate handler
                    for (int i = 0; event->text.text[i]; i++) {
                        app_controller_route_prompt_char(controller, app_state, event->text.text[i]);
                    }
                    break;
                }
                
                // Always process text input for normal terminal operation
                // Text input events contain the actual characters typed (letters, numbers, symbols)
                term_key_input(0, 0, event->text.text);
            }
            break;

        default:
            break;
    }
    
    return true;
}

// Input routing - simplified to delegate everything to input system
void app_controller_route_prompt_char(app_controller_t* controller, 
                                     app_state_t* app_state, 
                                     char ch) {
    if (!controller || !app_state) {
        return;
    }
    
    // Handle Enter key
    if (ch == '\r' || ch == '\n') {
        input_system_handle_enter(app_state);
        return;
    }
    
    // Handle other input
    input_system_handle_char(app_state, ch);
}

// Term write callback
static void term_write_callback(const uint8_t* data, size_t len, void* user) {
    app_state_t* app = (app_state_t*)user;
    
    if (!app) {
        fprintf(stderr, "term_write: null app state\n");
        return;
    }
    
    // Delegate all terminal output handling to input_system
    input_system_handle_terminal_output(app, data, len);
}

void app_controller_transition_to_mode(app_state_t* app, input_mode_t mode) {
    if (!app) {
        return;
    }
    app->input_mode = mode;
}

void app_controller_return_to_startup(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Clear connection state before showing startup menu
    ssh_manager_clear_connection_input(app);
    
    // Show startup menu through ui_manager
    app_controller_show_startup_menu(app);
}

void app_controller_handle_startup_choice(app_state_t* app, int choice) {
    if (!app) {
        return;
    }
    
    // Reset input mode  
    app->input_mode = INPUT_MODE_NORMAL;
    
    if (choice == 1) {
        // Start SSH connection setup
        ssh_manager_start_ui_sequence(app);
    } else if (choice == 2) {
        // Start test mode
        test_mode_init();
    }
}

void app_controller_handle_test_mode_input(const char *data, size_t len) {
    test_mode_handle_input((const uint8_t*)data, len);
}

void app_controller_handle_field_submit(app_state_t* app, input_mode_t field_mode) {
    if (!app) {
        return;
    }
    
    // Delegate to appropriate domain module based on field type
    switch (field_mode) {
        case INPUT_MODE_STARTUP_CHOICE:
            // Already handled by app_controller_handle_startup_choice
            break;
        case INPUT_MODE_HOSTNAME:
        case INPUT_MODE_USERNAME:
        case INPUT_MODE_PORT:
        case INPUT_MODE_PASSWORD:
            {
                // Handle SSH field submission
                app_result_t result = ssh_manager_handle_field_submit(app, field_mode);
                if (result == APP_RESULT_RETRY || result == APP_RESULT_CONTINUE) {
                    // Show the prompt for current or next field
                    ui_manager_display_current_prompt(app);
                }
            }
            break;
        case INPUT_MODE_DISCONNECT_PROMPT:
            {
                // Handle disconnect prompt - Enter key was pressed
                char enter_input[2] = { '\r', '\0' };
                app_controller_handle_disconnect_prompt(app, enter_input);
            }
            break;
        default:
            break;
    }
}

void app_controller_handle_disconnect_prompt(app_state_t* app, const char* input) {
    if (!app) {
        return;
    }
    
    // Handle disconnect prompt
    app_result_t result = ssh_manager_handle_disconnect_prompt(app, input);
    if (result == APP_RESULT_CANCEL) {
        // Return to startup menu (successful connection that later disconnected)
        app_controller_return_to_startup(app);
    } else if (result == APP_RESULT_RETRY) {
        // Restart SSH connection sequence (connection failed, try again)
        // ssh_manager already handled restarting the sequence
    }
}

void app_controller_handle_escape_key(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Business logic: decide what to do based on current mode
    switch (app->input_mode) {
        case INPUT_MODE_STARTUP_CHOICE:
            // Already at main menu, do nothing
            break;
        case INPUT_MODE_NORMAL:
            // In normal mode, check connection status
            if (ssh_manager_is_connected(app)) {
                // Connected to SSH - disconnect and show disconnect prompt
                ssh_manager_disconnect(app);
            } else {
                // Not connected - return to startup menu
                app_controller_return_to_startup(app);
            }
            break;
        case INPUT_MODE_HOSTNAME:
        case INPUT_MODE_USERNAME:
        case INPUT_MODE_PORT:
        case INPUT_MODE_PASSWORD:
        case INPUT_MODE_DISCONNECT_PROMPT:
            // Cancel current operation and return to startup menu
            ssh_manager_cleanup(app);
            app_controller_return_to_startup(app);
            break;
        default:
            // Fallback: return to startup menu
            app_controller_return_to_startup(app);
            break;
    }
}

void app_controller_handle_terminal_output(app_state_t* app, const uint8_t* data, size_t len) {
    if (!app || !data || len == 0) {
        return;
    }
    
    // Business logic: route terminal output based on connection state
    if (ssh_manager_is_connected(app)) {
        // Send data to SSH connection
        if (!ssh_manager_send_data(app, (const char*)data, len)) {
            // Error handling is done inside ssh_manager_send_data
        }
        return;
    }
    
    // Demo mode: Echo user input back for testing (when not connected to SSH)
    if (app->input_mode == INPUT_MODE_NORMAL) {
        app_controller_handle_test_mode_input((const char*)data, len);
    }
}

void app_controller_display_current_prompt(app_state_t* app) {
    if (!app) {
        return;
    }
    ui_manager_display_current_prompt(app);
}

void app_controller_show_startup_menu(app_state_t* app) {
    if (!app) {
        return;
    }
    ui_manager_show_startup_menu(app);
}
