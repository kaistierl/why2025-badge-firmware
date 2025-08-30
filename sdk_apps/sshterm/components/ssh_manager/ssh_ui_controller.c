/**
 * @file ssh_ui_controller.c
 * @brief SSH User Interface Controller Implementation
 * 
 * This component handles all SSH-related UI flows, input processing,
 * and user interaction sequences. It provides clean separation between
 * UI concerns and SSH connection management.
 */

#include "ssh_ui_controller.h"
#include "ssh_manager.h"
#include "../ui_manager/ui_manager.h"
#include "../term/term.h"
#include <string.h>
#include <stdlib.h>

// Forward declaration for internal helper
static void ssh_ui_display_disconnect_prompt_internal(app_state_t* app);

void ssh_ui_start_connection_sequence(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Clear previous connection data
    ssh_ui_clear_connection_input(app);
    
    // Pre-populate fields with default values so they're visible immediately
    ssh_ui_apply_field_defaults(app, INPUT_MODE_PORT);
    
    // Set initial input mode
    app->input_mode = INPUT_MODE_HOSTNAME;
    
    // Display SSH connection setup UI
    ui_manager_show_ssh_connection_setup(app);
}

void ssh_ui_progress_to_next_field(app_state_t* app) {
    if (!app) {
        return;
    }
    
    switch (app->input_mode) {
        case INPUT_MODE_HOSTNAME:
            app->input_mode = INPUT_MODE_USERNAME;
            // Initialize username field length from existing content
            app->connection_input.field_lengths.username = strlen(app->connection_input.username);
            break;
            
        case INPUT_MODE_USERNAME:
            app->input_mode = INPUT_MODE_PORT;
            // Initialize port field length from existing content
            app->connection_input.field_lengths.port = strlen(app->connection_input.port_str);
            break;
            
        case INPUT_MODE_PORT:
            app->input_mode = INPUT_MODE_PASSWORD;
            // Initialize password field length from existing content
            app->connection_input.field_lengths.password = strlen(app->connection_input.password);
            break;
            
        default:
            // No progression for other modes
            break;
    }
    
    // Add newline before displaying the prompt for the new field
    term_input_string("\r\n");
    
    // Display the prompt for the new field
    ui_manager_display_current_prompt(app);
}

void ssh_ui_apply_field_defaults(app_state_t* app, input_mode_t field_mode) {
    if (!app) {
        return;
    }
    
    switch (field_mode) {
        case INPUT_MODE_PORT:
            if (app->connection_input.field_lengths.port == 0) {
                strncpy(app->connection_input.port_str, "22", 
                       sizeof(app->connection_input.port_str) - 1);
                app->connection_input.port_str[sizeof(app->connection_input.port_str) - 1] = '\0';
                app->connection_input.field_lengths.port = strlen(app->connection_input.port_str);
            }
            break;
            
        default:
            // No defaults for other modes
            break;
    }
}

app_result_t ssh_ui_handle_field_submit(app_state_t* app, input_mode_t field_mode) {
    if (!app) {
        return APP_RESULT_ERROR;
    }
    
    switch (field_mode) {
        case INPUT_MODE_HOSTNAME:
            ssh_ui_apply_field_defaults(app, INPUT_MODE_HOSTNAME);
            ssh_ui_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_USERNAME:
            ssh_ui_apply_field_defaults(app, INPUT_MODE_USERNAME);
            ssh_ui_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_PORT:
            ssh_ui_apply_field_defaults(app, INPUT_MODE_PORT);
            ssh_ui_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_PASSWORD:
            return ssh_ui_attempt_connection(app);
            
        default:
            // Not an SSH field - return error to indicate it's not handled here
            return APP_RESULT_ERROR;
    }
}

app_result_t ssh_ui_handle_disconnect_prompt(app_state_t* app, const char* input) {
    if (!app || !input) {
        return APP_RESULT_ERROR;
    }
    
    if (app->input_mode != INPUT_MODE_DISCONNECT_PROMPT) {
        return APP_RESULT_CONTINUE;
    }
    
    // Handle Enter key - return to main menu or restart SSH sequence
    if (*input == '\r' || *input == '\n') {
        term_input_string("\r\n");
        // Check if we have partial connection input (indicating this was an SSH connection failure)
        if (strlen(app->connection_input.hostname) > 0 || 
            strlen(app->connection_input.username) > 0 || 
            strlen(app->connection_input.port_str) > 0) {
            // Connection failed - restart SSH input sequence to try again
            ssh_ui_start_connection_sequence(app);
            return APP_RESULT_RETRY;
        } else {
            // Successful connection that later disconnected - return to main menu with clean state
            ssh_manager_cleanup(app);
            // Signal app controller to return to startup menu
            return APP_RESULT_CANCEL;
        }
    }
    
    // Ignore all other input - only Enter is meaningful
    return APP_RESULT_CONTINUE;
}

void ssh_ui_clear_connection_input(app_state_t* app) {
    if (!app) {
        return;
    }
    memset(&app->connection_input, 0, sizeof(connection_input_t));
    app->input_mode = INPUT_MODE_NORMAL;
}

void ssh_ui_handle_connection_success(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Clear all connection input data and transition to normal terminal mode
    ssh_ui_clear_connection_input(app);
}

app_result_t ssh_ui_attempt_connection(app_state_t* app) {
    if (!app) {
        return APP_RESULT_ERROR;
    }
    
    // Validate required fields
    if (app->connection_input.field_lengths.password == 0) {
        ui_manager_show_validation_error("Password cannot be empty!");
        return APP_RESULT_RETRY;
    }
    
    // Parse port with default fallback
    int port = atoi(app->connection_input.port_str);
    if (port <= 0 || port > 65535) {
        port = 22;
    }
    
    // Attempt connection via SSH manager
    return ssh_manager_connect(app, 
                              app->connection_input.hostname, 
                              port,
                              app->connection_input.username, 
                              app->connection_input.password) 
           ? APP_RESULT_SUCCESS 
           : APP_RESULT_ERROR;
}

void ssh_ui_display_disconnect_prompt(app_state_t* app) {
    ssh_ui_display_disconnect_prompt_internal(app);
}

// Internal helper function
static void ssh_ui_display_disconnect_prompt_internal(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Switch to disconnect prompt mode
    app->input_mode = INPUT_MODE_DISCONNECT_PROMPT;
    
    // Show the prompt
    ui_manager_display_current_prompt(app);
}
