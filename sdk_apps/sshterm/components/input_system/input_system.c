#include "input_system.h"
#include "../app_controller/app_controller.h"
#include "../../common/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for functions we'll need to implement
static void handle_startup_choice_submit(app_state_t* app);

// Input system initialization
bool input_system_init(void) {
    // Currently no initialization needed
    return true;
}

void input_system_shutdown(void) {
    // Currently no cleanup needed
}

// Get current input field configuration
input_field_t input_system_get_current_field(app_state_t* app) {
    input_field_t field = {0};
    
    switch (app->input_mode) {
        case INPUT_MODE_STARTUP_CHOICE:
            field = (input_field_t){
                .buffer = app->connection_input.startup_choice,
                .length = &app->connection_input.field_lengths.startup_choice,
                .max_length = sizeof(app->connection_input.startup_choice) - 1,
                .prompt = "Choice: ",
                .default_value = NULL,
                .is_password = false,
                .numeric_only = false
            };
            break;
        case INPUT_MODE_HOSTNAME:
            field = (input_field_t){
                .buffer = app->connection_input.hostname,
                .length = &app->connection_input.field_lengths.hostname,
                .max_length = sizeof(app->connection_input.hostname) - 1,
                .prompt = "Hostname: ",
                .default_value = "NULL",
                .is_password = false,
                .numeric_only = false
            };
            break;
        case INPUT_MODE_USERNAME:
            field = (input_field_t){
                .buffer = app->connection_input.username,
                .length = &app->connection_input.field_lengths.username,
                .max_length = sizeof(app->connection_input.username) - 1,
                .prompt = "Username: ",
                .default_value = "NULL",
                .is_password = false,
                .numeric_only = false
            };
            break;
        case INPUT_MODE_PORT:
            field = (input_field_t){
                .buffer = app->connection_input.port_str,
                .length = &app->connection_input.field_lengths.port,
                .max_length = sizeof(app->connection_input.port_str) - 1,
                .prompt = "Port: ",
                .default_value = "22",
                .is_password = false,
                .numeric_only = true
            };
            break;
        case INPUT_MODE_PASSWORD:
            field = (input_field_t){
                .buffer = app->connection_input.password,
                .length = &app->connection_input.field_lengths.password,
                .max_length = sizeof(app->connection_input.password) - 1,
                .prompt = "Password: ",
                .default_value = NULL,
                .is_password = true,
                .numeric_only = false
            };
            break;
        default:
            break;
    }
    
    return field;
}

// Handle character input
void input_system_handle_char(app_state_t* app, char ch) {
    // Handle backspace
    if (ch == '\b' || ch == 127) {
        input_field_t field = input_system_get_current_field(app);
        if (field.length && *field.length > 0) {
            field.buffer[--(*field.length)] = '\0';
            input_system_display_prompt(app);
        }
        return;
    }
    
    // Handle printable characters
    if (ch >= 32 && ch <= 126) {
        input_field_t field = input_system_get_current_field(app);
        if (!field.buffer || !field.length) {
            return;
        }
        
        // Check numeric constraint
        if (field.numeric_only && (ch < '0' || ch > '9')) {
            return; // Ignore non-numeric input
        }
        
        // Check buffer space
        if ((size_t)*field.length < field.max_length) {
            field.buffer[*field.length] = ch;
            field.buffer[*field.length + 1] = '\0';
            (*field.length)++;
            input_system_display_prompt(app);
        }
    }
}

// Handle Enter key
void input_system_handle_enter(app_state_t* app) {
    input_field_t field = input_system_get_current_field(app);
    
    // Apply default value if field is empty and has a default
    if (field.length && *field.length == 0 && field.default_value) {
        strncpy(field.buffer, field.default_value, field.max_length);
        field.buffer[field.max_length] = '\0';
        *field.length = strlen(field.buffer);
    }
    
    // Field-specific validation and next step - delegate to app_controller
    switch (app->input_mode) {
        case INPUT_MODE_STARTUP_CHOICE:
            handle_startup_choice_submit(app);
            break;
        default:
            // Delegate all field submission to app_controller
            app_controller_handle_field_submit(app, app->input_mode);
            break;
    }
}

// Display current prompt - delegate to UI manager
void input_system_display_prompt(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Pure delegation to UI layer
    app_controller_display_current_prompt(app);
}

// Clear input state
void input_system_clear_state(app_state_t* app) {
    memset(&app->connection_input, 0, sizeof(connection_input_t));
    app->input_mode = INPUT_MODE_NORMAL;
}

static void handle_startup_choice_submit(app_state_t* app) {
    if (!app) {
        return;
    }
    
    int len = app->connection_input.field_lengths.startup_choice;
    char* input = app->connection_input.startup_choice;
    int choice = 0;
    
    // Handle numeric input (1 or 2)
    if (len == 1 && input[0] >= '1' && input[0] <= '2') {
        choice = input[0] - '0';
    }
    // Handle text input (test, ssh)
    else if (len >= 3) {
        if (strncmp(input, "test", 4) == 0) {
            choice = 2;
        } else if (strncmp(input, "ssh", 3) == 0) {
            choice = 1;
        }
    }
    
    if (choice > 0) {
        // Delegate business logic to app_controller
        app_controller_handle_startup_choice(app, choice);
    }
}

void input_system_show_startup_menu(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Set input mode
    app->input_mode = INPUT_MODE_STARTUP_CHOICE;
    
    // Delegate UI display to ui_manager
    app_controller_show_startup_menu(app);
}

void input_system_handle_escape_key(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Delegate escape key handling to app_controller
    app_controller_handle_escape_key(app);
}

void input_system_handle_terminal_output(app_state_t* app, const uint8_t* data, size_t len) {
    if (!app) {
        return;
    }
    
    // Only process terminal data in NORMAL mode. Prompt input is handled via SDL events.
    if (app->input_mode != INPUT_MODE_NORMAL) {
        return;
    }
    
    // Delegate terminal output handling to app_controller
    app_controller_handle_terminal_output(app, data, len);
}
