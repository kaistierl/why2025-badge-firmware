#include "ssh_manager.h"
#include "../ssh_client/ssh_client.h"
#include "../term/term.h"
#include "../ui_manager/ui_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations
static void ssh_manager_display_disconnect_prompt(app_state_t* app);
static bool ssh_manager_handle_connection_completion(app_state_t* app);
static void ssh_manager_handle_disconnect(app_state_t* app, const char* message, bool show_in_terminal);

// SSH Manager implementation

bool ssh_manager_init(void) {
    // Initialize any global SSH manager state if needed
    return true;
}

void ssh_manager_shutdown(void) {
    // Cleanup any global SSH manager state if needed
}

bool ssh_manager_connect(app_state_t* app, const char* hostname, int port, 
                        const char* username, const char* password) {
    if (!app || !hostname || !username || !password) {
        return false;
    }
    
    ui_manager_show_connecting_message();
    
    // Validate and set default port if needed
    int safe_port = (port > 0 && port <= 65535) ? port : 22;
    
    // Initialize SSH client
    if (!ssh_client_init(&app->ssh_client)) {
        ui_manager_show_connection_error("Failed to initialize SSH client");
        ssh_manager_display_disconnect_prompt(app);
        return false;
    }
    
    // Start non-blocking connection
    if (!ssh_client_connect_start(&app->ssh_client, hostname, safe_port, username, password)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "SSH connection failed to start: %s", 
                ssh_client_get_error(&app->ssh_client));
        ui_manager_show_connection_error(error_msg);
        ssh_client_cleanup(&app->ssh_client);
        ssh_manager_display_disconnect_prompt(app);
        return false;
    }
    
    // Mark as connecting - ssh_manager_poll_and_read() will handle progress
    app->ssh_connecting = true;
    return true;
}

void ssh_manager_disconnect(app_state_t* app) {
    if (!app) {
        return;
    }
    
    if (app->ssh_connected || app->ssh_connecting) {
        ssh_client_cleanup(&app->ssh_client);
    }
    
    app->ssh_connected = false;
    app->ssh_connecting = false;
}

bool ssh_manager_is_connected(app_state_t* app) {
    return app && app->ssh_connected && ssh_client_is_connected(&app->ssh_client);
}

bool ssh_manager_send_data(app_state_t* app, const char* data, size_t len) {
    if (!ssh_manager_is_connected(app)) {
        return false;
    }
    
    if (!ssh_client_send(&app->ssh_client, data, len)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to send data: %s", 
                ssh_client_get_error(&app->ssh_client));
        ssh_manager_handle_disconnect(app, error_msg, false);
        return false;
    }
    
    return true;
}

// Helper function to handle connection completion
static bool ssh_manager_handle_connection_completion(app_state_t* app) {
    ssh_state_t state = ssh_client_get_state(&app->ssh_client);
    
    if (state == SSH_STATE_CONNECTED) {
        // Connection successful
        app->ssh_connected = true;
        app->ssh_connecting = false;
        app->had_ssh_session = true;
        
        ui_manager_show_connection_success(app->ssh_client.hostname, app->ssh_client.username);
        ssh_manager_clear_connection_input(app);
        return true;
    } else {
        // Connection failed
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "SSH connection failed: %s", 
                ssh_client_get_error(&app->ssh_client));
        ui_manager_show_connection_error(error_msg);
        ssh_client_cleanup(&app->ssh_client);
        app->ssh_connecting = false;
        ssh_manager_display_disconnect_prompt(app);
        return false;
    }
}

bool ssh_manager_poll_and_read(app_state_t* app) {
    if (!app) {
        return false;
    }
    
    // Handle ongoing connection process
    if (app->ssh_connecting) {
        if (ssh_client_connect_continue(&app->ssh_client)) {
            // Still connecting, continue polling
            return false;
        } else {
            // Connection completed (either success or failure)
            return ssh_manager_handle_connection_completion(app);
        }
    }
    
    // If not connected, nothing to read
    if (!ssh_manager_is_connected(app)) {
        return false;
    }
    
    // Read data from SSH connection
    char buffer[4096];
    int bytes_read = ssh_client_receive(&app->ssh_client, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        // Forward data to terminal
        term_input_bytes((const uint8_t*)buffer, (size_t)bytes_read);
        return true;
    } else if (bytes_read == -2) {
        // Clean disconnect
        ssh_manager_handle_disconnect(app, ssh_client_get_error(&app->ssh_client), true);
        return false;
    } else if (bytes_read < 0) {
        // Connection error
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "SSH read error: %s", 
                ssh_client_get_error(&app->ssh_client));
        ssh_manager_handle_disconnect(app, error_msg, true);
        return false;
    }
    
    // No data available (bytes_read == 0)
    return false;
}

// Helper function to handle disconnection with consistent cleanup
static void ssh_manager_handle_disconnect(app_state_t* app, const char* message, bool show_in_terminal) {
    if (!app || !message) {
        return;
    }
    
    // Output to console
    printf("%s\n", message);
    
    // Show in terminal if requested
    if (show_in_terminal) {
        term_input_string("\r\n");
        term_input_string(message);
        if (message[strlen(message) - 1] != '\n') {
            term_input_string("\r\n");
        }
    }
    
    // Cleanup and show disconnect prompt
    ssh_manager_cleanup(app);
    ssh_manager_clear_connection_input(app);
    ssh_manager_display_disconnect_prompt(app);
}

bool ssh_manager_is_connecting(app_state_t* app) {
    return app && app->ssh_connecting;
}

const char* ssh_manager_get_error(app_state_t* app) {
    if (!app) {
        return "Invalid app state";
    }
    return ssh_client_get_error(&app->ssh_client);
}

void ssh_manager_cleanup(app_state_t* app) {
    if (!app) {
        return;
    }
    
    if (app->ssh_connected || app->ssh_connecting) {
        ssh_client_cleanup(&app->ssh_client);
    }
    
    app->ssh_connected = false;
    app->ssh_connecting = false;
}

void ssh_manager_clear_connection_input(app_state_t* app) {
    if (!app) {
        return;
    }
    memset(&app->connection_input, 0, sizeof(connection_input_t));
    app->input_mode = INPUT_MODE_NORMAL;
}

// Forward declaration for display_disconnect_prompt - this will be refactored later
static void ssh_manager_display_disconnect_prompt(app_state_t* app) {
    // Switch to disconnect prompt mode
    app->input_mode = INPUT_MODE_DISCONNECT_PROMPT;
    
    // Show the prompt
    ui_manager_display_current_prompt(app);
}

app_result_t ssh_manager_attempt_connection(app_state_t* app) {
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
    
    // Attempt connection
    return ssh_manager_connect(app, 
                              app->connection_input.hostname, 
                              port,
                              app->connection_input.username, 
                              app->connection_input.password) 
           ? APP_RESULT_SUCCESS 
           : APP_RESULT_ERROR;
}

void ssh_manager_progress_to_next_field(app_state_t* app) {
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

void ssh_manager_apply_field_defaults(app_state_t* app, input_mode_t field_mode) {
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

void ssh_manager_start_ui_sequence(app_state_t* app) {
    if (!app) {
        return;
    }
    
    // Pure business logic - no UI formatting
    ssh_manager_clear_connection_input(app);
    
    // Pre-populate fields with default values so they're visible immediately
    ssh_manager_apply_field_defaults(app, INPUT_MODE_HOSTNAME);
    ssh_manager_apply_field_defaults(app, INPUT_MODE_USERNAME);
    ssh_manager_apply_field_defaults(app, INPUT_MODE_PORT);
    
    // Set initial input mode
    app->input_mode = INPUT_MODE_HOSTNAME;
    
    // Display SSH connection setup UI
    ui_manager_show_ssh_connection_setup(app);
}

app_result_t ssh_manager_handle_disconnect_prompt(app_state_t* app, const char* input) {
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
            ssh_manager_start_ui_sequence(app);
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

app_result_t ssh_manager_handle_field_submit(app_state_t* app, input_mode_t field_mode) {
    if (!app) {
        return APP_RESULT_ERROR;
    }
    
    switch (field_mode) {
        case INPUT_MODE_HOSTNAME:
            ssh_manager_apply_field_defaults(app, INPUT_MODE_HOSTNAME);
            ssh_manager_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_USERNAME:
            ssh_manager_apply_field_defaults(app, INPUT_MODE_USERNAME);
            ssh_manager_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_PORT:
            ssh_manager_apply_field_defaults(app, INPUT_MODE_PORT);
            ssh_manager_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;
            
        case INPUT_MODE_PASSWORD:
            return ssh_manager_attempt_connection(app);
            
        default:
            // Not an SSH field - return error to indicate it's not handled here
            return APP_RESULT_ERROR;
    }
}
