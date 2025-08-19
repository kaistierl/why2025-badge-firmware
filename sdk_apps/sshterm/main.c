#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "components/renderer/renderer.h"
#include "components/term/term.h"
#include "components/term/terminal_config.h"
#include "components/keyboard/keyboard.h"

// compile time fullscreen flag
#if defined(BADGEVMS_FULLSCREEN) && BADGEVMS_FULLSCREEN
    #define WIN_FULLSCREEN 1
#else
    #define WIN_FULLSCREEN 0
#endif
static const int WIN_W = 720;
static const int WIN_H = 720;

// --- Application state structure for SSH integration
typedef struct {
    bool ssh_connected;
    bool ssh_connecting;
    // TODO: Add SSH client handle when SSH component is implemented
    // ssh_client_t* ssh_client;
} app_state_t;

// Forward declarations
static void demo_echo_input(const uint8_t* data, size_t len);
static void display_color_test(void);
static bool ssh_init_connection(app_state_t* app, const char* host, int port, const char* username);
static void ssh_cleanup(app_state_t* app);
static bool ssh_poll_and_read(app_state_t* app);

// --- Write callback: terminal → SSH host
static void term_write(const uint8_t* data, size_t len, void* user) {
    app_state_t* app = (app_state_t*)user;
    
    if (!app) {
        fprintf(stderr, "term_write: null app state\n");
        return;
    }
    
    // TODO: When SSH is implemented, send data to SSH connection
    // if (app->ssh_connected && app->ssh_client) {
    //     ssh_write(app->ssh_client, data, len);
    //     return;
    // }
    
    // Demo mode: Echo user input back for testing (REMOVE when SSH added)
    // This simulates a basic shell echo for testing terminal functionality
    if (len > 0 && !app->ssh_connected) {
        demo_echo_input(data, len);
    }
}

// --- Demo echo function (REMOVE when SSH implemented)
static void demo_echo_input(const uint8_t* data, size_t len) {
    // Echo back characters (simulate a basic shell echo)
    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 32 && data[i] <= 126) {
            // Echo printable characters back to terminal
            term_input_bytes(&data[i], 1);
        } else if (data[i] == '\r') {
            // Echo carriage return + line feed
            const char* crlf = "\r\n";
            term_input_bytes((const uint8_t*)crlf, 2);
        } else if (data[i] == '\t') {
            // Echo tab as spaces (4 spaces)
            const char* spaces = "    ";
            term_input_bytes((const uint8_t*)spaces, 4);
        } else if (data[i] == '\b' || data[i] == 127) {
            // For backspace, send backspace + space + backspace to erase character
            const char* erase = "\b \b";
            term_input_bytes((const uint8_t*)erase, 3);
        } else if (data[i] == 3) {
            // Ctrl+C - show visual feedback
            const char* msg = " [CTRL+C] ";
            term_input_bytes((const uint8_t*)msg, strlen(msg));
        } else if (data[i] == 27 && i + 1 < len && data[i+1] >= 'a' && data[i+1] <= 'z') {
            // Alt + lowercase letter combination (ESC + letter)
            char alt_msg[16];
            snprintf(alt_msg, sizeof(alt_msg), " [ALT+%c] ", data[i+1] - 'a' + 'A');
            term_input_bytes((const uint8_t*)alt_msg, strlen(alt_msg));
            i++; // Skip the next character since we processed it
        } else if (data[i] == 27 && i + 1 < len && data[i+1] >= 'A' && data[i+1] <= 'Z') {
            // Alt + uppercase letter combination (ESC + letter with shift)
            char alt_msg[16];
            snprintf(alt_msg, sizeof(alt_msg), " [ALT+SHIFT+%c] ", data[i+1]);
            term_input_bytes((const uint8_t*)alt_msg, strlen(alt_msg));
            i++; // Skip the next character since we processed it
        } else if (data[i] == 27 && (i + 2 >= len || data[i+1] != '[')) {
            // ESC - show visual feedback only if it's not part of an escape sequence
            // Most terminal escape sequences start with ESC[ so we avoid those
            const char* msg = " [ESC] ";
            term_input_bytes((const uint8_t*)msg, strlen(msg));
        } else if (data[i] == 27) {
            // This is likely an escape sequence, pass it through normally
            term_input_bytes(&data[i], 1);
        } else if (data[i] < 32) {
            // Other control characters - show as [CTRL+X]
            char ctrl_msg[16];
            snprintf(ctrl_msg, sizeof(ctrl_msg), " [CTRL+%c] ", 'A' + data[i] - 1);
            term_input_bytes((const uint8_t*)ctrl_msg, strlen(ctrl_msg));
        }
    }
}

// --- SSH Integration placeholder functions (implement when SSH component ready)
static bool ssh_init_connection(app_state_t* app, const char* host, int port, const char* username) {
    (void)app; (void)host; (void)port; (void)username;
    // TODO: Initialize SSH connection
    // app->ssh_client = ssh_new(host, port, username, password, RENDER_COLS, RENDER_ROWS, "xterm-256color");
    // return app->ssh_client != NULL;
    return false;
}

static void ssh_cleanup(app_state_t* app) {
    (void)app;
    // TODO: Clean up SSH connection
    // if (app->ssh_client) {
    //     ssh_free(app->ssh_client);
    //     app->ssh_client = NULL;
    // }
    app->ssh_connected = false;
    app->ssh_connecting = false;
}

static bool ssh_poll_and_read(app_state_t* app) {
    (void)app;
    // TODO: Poll SSH socket and read data
    // if (!app->ssh_connected || !app->ssh_client) return false;
    // 
    // uint8_t buffer[4096];
    // ssize_t n = ssh_read(app->ssh_client, buffer, sizeof(buffer));
    // if (n > 0) {
    //     term_input_bytes(buffer, (size_t)n);
    //     return true;
    // } else if (n < 0) {
    //     // Connection error
    //     ssh_cleanup(app);
    //     return false;
    // }
    return false;
}

// --- Color and bold text test function
static void display_color_test(void) {
    // Terminal title and info
    term_input_bytes((const uint8_t*)"\x1b[1;36m", 7);  // Bold cyan
    term_input_bytes((const uint8_t*)"=== SSHTerm Test Suite ===\r\n", 30);
    term_input_bytes((const uint8_t*)"\x1b[0m", 4);  // Reset
    
    term_input_bytes((const uint8_t*)"libvterm-0.3.3 | 80x39 grid | Leggie 9x18 font\r\n", 48);
    term_input_bytes((const uint8_t*)"Type text to test. Ctrl+Q to quit.\r\n\r\n", 38);
    
    // 8-color ANSI test
    term_input_bytes((const uint8_t*)"\x1b[1mANSI Colors (8-color palette):\x1b[0m\r\n", 40);
    
    // Standard colors
    const char* colors[] = {
        "\x1b[30m Black ", "\x1b[31m Red ", "\x1b[32m Green ", "\x1b[33m Yellow ",
        "\x1b[34m Blue ", "\x1b[35m Magenta ", "\x1b[36m Cyan ", "\x1b[37m White "
    };
    
    for (int i = 0; i < 8; i++) {
        term_input_bytes((const uint8_t*)colors[i], strlen(colors[i]));
    }
    term_input_bytes((const uint8_t*)"\x1b[0m\r\n", 6);  // Reset + newline
    
    // Bold colors test
    term_input_bytes((const uint8_t*)"\x1b[1mBold Colors (brightness enhanced):\x1b[0m\r\n", 43);
    for (int i = 0; i < 8; i++) {
        term_input_bytes((const uint8_t*)"\x1b[1m", 4);  // Bold
        term_input_bytes((const uint8_t*)colors[i], strlen(colors[i]));
        term_input_bytes((const uint8_t*)"\x1b[0m", 4);  // Reset
    }
    term_input_bytes((const uint8_t*)"\r\n", 2);
    
    // Background colors test
    term_input_bytes((const uint8_t*)"\x1b[1mBackground Colors:\x1b[0m\r\n", 28);
    const char* bg_colors[] = {
        "\x1b[40;37m Black ", "\x1b[41;37m Red ", "\x1b[42;30m Green ", "\x1b[43;30m Yellow ",
        "\x1b[44;37m Blue ", "\x1b[45;37m Magenta ", "\x1b[46;30m Cyan ", "\x1b[47;30m White "
    };
    
    for (int i = 0; i < 8; i++) {
        term_input_bytes((const uint8_t*)bg_colors[i], strlen(bg_colors[i]));
    }
    term_input_bytes((const uint8_t*)"\x1b[0m\r\n", 6);  // Reset + newline
    
    // Text attributes test
    term_input_bytes((const uint8_t*)"\r\n\x1b[1mText Attributes:\x1b[0m\r\n", 28);
    term_input_bytes((const uint8_t*)"Normal text | ", 14);
    term_input_bytes((const uint8_t*)"\x1b[1mBold text\x1b[0m | ", 20);
    term_input_bytes((const uint8_t*)"\x1b[4mUnderlined\x1b[0m | ", 21);
    term_input_bytes((const uint8_t*)"\x1b[7mReversed\x1b[0m\r\n", 18);
    
    // RGB color test (if supported)
    term_input_bytes((const uint8_t*)"\r\n\x1b[1mRGB Colors (24-bit):\x1b[0m\r\n", 32);
    term_input_bytes((const uint8_t*)"\x1b[38;2;255;100;50mOrange\x1b[0m ", 30);
    term_input_bytes((const uint8_t*)"\x1b[38;2;100;255;100mLime\x1b[0m ", 28);
    term_input_bytes((const uint8_t*)"\x1b[38;2;100;100;255mSky\x1b[0m ", 27);
    term_input_bytes((const uint8_t*)"\x1b[38;2;255;50;255mPink\x1b[0m\r\n", 28);
    
    // Character set test
    term_input_bytes((const uint8_t*)"\x1b[1mASCII Test (32-126):\x1b[0m\r\n", 30);
    term_input_bytes((const uint8_t*)"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnop\r\n", 87);
    term_input_bytes((const uint8_t*)"qrstuvwxyz{|}~\r\n", 18);
    
    // Grid test
    term_input_bytes((const uint8_t*)"\r\n\x1b[1mGrid Test (80 columns):\x1b[0m\r\n", 35);
    // Exactly 80 characters (0-79 positions)
    term_input_bytes((const uint8_t*)"01234567890123456789012345678901234567890123456789012345678901234567890123456789\r\n", 82);
    term_input_bytes((const uint8_t*)"          1         2         3         4         5         6         7         \r\n", 82);
    
    term_input_bytes((const uint8_t*)"\r\n\x1b[32m>>> Ready for input! <<<\x1b[0m\r\n", 38);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // Initialize application state
    app_state_t app_state = {
        .ssh_connected = false,
        .ssh_connecting = false
    };

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    Uint32 win_flags = WIN_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0;
    SDL_Window* win = SDL_CreateWindow("SSH Terminal", WIN_W, WIN_H, win_flags);

    if (!win) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    renderer_init(win, ren);

    // Enable SDL text input events
    SDL_StartTextInput(win);

    // Init terminal emulator with app state as user context
    if (!term_init(RENDER_COLS, RENDER_ROWS, term_write, &app_state)) {
        fprintf(stderr, "term_init failed\n");
        renderer_shutdown();
        SDL_StopTextInput(win);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // Display comprehensive color and feature test
    // Tests: 8-color ANSI, bold colors, background colors, text attributes,
    // 24-bit RGB colors, ASCII character set, and 80-column grid layout
    display_color_test();

    bool running = true;
    SDL_Event ev;
    
    while (running) {
        // Process all pending events without blocking
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                handle_key_event(&ev.key, &running);
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (ev.text.text && *ev.text.text) {
                    // Only process text input if not a control character
                    // (Ctrl combinations should be handled by KEY_DOWN events)
                    bool is_ctrl_combo = false;
                    for (const char* p = ev.text.text; *p; p++) {
                        if (*p < 32) {  // Control character
                            is_ctrl_combo = true;
                            break;
                        }
                    }
                    if (!is_ctrl_combo) {
                        term_key_input(0, 0, ev.text.text);
                    }
                }
                break;

            default:
                break;
            }
        }

        // TODO: Poll SSH connection for data when implemented
        // ssh_poll_and_read(&app_state);
        
        // Update screen if needed (handles cursor blinking internally)
        renderer_present_if_dirty(SDL_GetTicks());
        
        // Small delay to prevent excessive CPU usage while maintaining responsiveness
        SDL_Delay(16);  // ~60 FPS refresh rate
    }

    // Cleanup
    ssh_cleanup(&app_state);
    term_shutdown();
    renderer_shutdown();
    SDL_StopTextInput(win);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
