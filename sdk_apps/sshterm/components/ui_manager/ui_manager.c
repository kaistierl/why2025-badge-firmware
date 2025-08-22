#include "ui_manager.h"
#include "../term/term.h"
#include "../input_system/input_system.h"
#include "../renderer/renderer.h"
#include <stdio.h>
#include <string.h>

// UI Manager implementation

bool ui_manager_init(void) {
    // No initialization needed currently
    return true;
}

void ui_manager_shutdown(void) {
    // No cleanup needed currently
}

void ui_manager_clear_screen(void) {
    term_input_string("\x1b[2J\x1b[H");
}

void ui_manager_show_header(const char* title) {
    if (!title) {
        return;
    }
    
    char header[256];
    snprintf(header, sizeof(header), "\x1b[1;36m=== %s ===\x1b[0m\r\n", title);
    term_input_string(header);
}

void ui_manager_show_startup_menu(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Clear screen and show startup menu
    ui_manager_clear_screen();
    ui_manager_show_header("SSH Terminal Application");
    
    const char* options = "Choose mode:\r\n";
    term_input_string(options);
    
    const char* test_option = "  \x1b[33mtest\x1b[0m - Terminal test mode (colors, features)\r\n";
    term_input_string(test_option);
    
    const char* ssh_option = "  \x1b[33mssh\x1b[0m  - SSH connection mode\r\n\r\n";
    term_input_string(ssh_option);
    
    const char* quit_info = "Press Ctrl+Q to quit the application\r\n\r\n";
    term_input_string(quit_info);
    
    // Set input mode and display prompt
    app->input_mode = INPUT_MODE_STARTUP_CHOICE;
    
    // Show startup prompt
    term_input_string("\r\nChoice: ");
}

void ui_manager_show_ssh_connection_setup(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Clear screen and show SSH setup
    ui_manager_clear_screen();
    ui_manager_show_header("SSH Connection Setup");
    
    // Set initial input mode and display prompt
    app->input_mode = INPUT_MODE_HOSTNAME;
    input_field_t field = input_system_get_current_field(app);
    ui_manager_display_field_prompt(field);
}

void ui_manager_show_connection_cancelled_message(void) {
    term_input_string("\r\n\x1b[33m[Connection cancelled]\x1b[0m\r\n");
}

void ui_manager_show_test_mode_message(void) {
    const char* test_msg = "\r\n\x1b[1;33mTerminal test mode active. Type to test features. ESC for menu, Ctrl+Q to quit.\x1b[0m\r\n\r\n";
    term_input_string(test_msg);
}

void ui_manager_show_help_message(void) {
    term_input_string("\r\n\x1b[33mPlease choose 'test' or 'ssh' to continue.\x1b[0m\r\n");
}

void ui_manager_show_connecting_message(void) {
    term_input_string("\r\n\x1b[33mConnecting...\x1b[0m\r\n");
    // render immediately
    renderer_present_if_dirty(0);
}

void ui_manager_show_connection_error(const char* error) {
    if (!error) {
        return;
    }
    char error_msg[512];
    snprintf(error_msg, sizeof(error_msg), "\x1b[31m%s\x1b[0m\r\n", error);
    term_input_string(error_msg);
}

void ui_manager_show_validation_error(const char* error) {
    if (!error) {
        return;
    }
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "\r\n\x1b[31m%s\x1b[0m\r\n", error);
    term_input_string(error_msg);
}

void ui_manager_show_connection_success(const char* hostname, const char* username) {
    if (!hostname || !username) {
        return;
    }
    
    term_input_string("\x1b[2J\x1b[H");
    
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), 
             "\x1b[32mConnected to %s as %s\x1b[0m\r\n", hostname, username);
    term_input_string(success_msg);
}

void ui_manager_display_field_prompt(input_field_t field) {
    if (!field.buffer || !field.prompt) {
        return;
    }
    
    // Clear line and show prompt (no newline, just update current line)
    term_input_string("\r\x1b[K");
    term_input_string(field.prompt);
    
    // Show field content (masked if password)
    if (field.is_password) {
        char password_display[256];
        int safe_len = (*field.length < (int)sizeof(password_display) - 1) ? 
                      *field.length : (int)sizeof(password_display) - 1;
        for (int i = 0; i < safe_len; i++) {
            password_display[i] = '*';
        }
        password_display[safe_len] = '\0';
        term_input_string(password_display);
    } else {
        term_input_string(field.buffer);
    }
}

void ui_manager_display_current_prompt(app_state_t* app) {
    if (!app) {
        return;
    }
    
    if (app->input_mode == INPUT_MODE_DISCONNECT_PROMPT) {
        // Disconnect prompt - different message based on whether connection failed or succeeded then disconnected
        const char* prompt;
        if (strlen(app->connection_input.hostname) > 0 || 
            strlen(app->connection_input.username) > 0 || 
            strlen(app->connection_input.port_str) > 0) {
            prompt = "Press Enter to try SSH connection again...";
        } else {
            prompt = "Press Enter to return to main menu...";
        }
        term_input_string("\r\n\r\x1b[K");
        term_input_string(prompt);
        return;
    }
    
    input_field_t field = input_system_get_current_field(app);
    ui_manager_display_field_prompt(field);
}
