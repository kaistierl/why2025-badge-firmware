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

    // Clear previous connection data and ensure clean state
    ssh_ui_clear_connection_input(app);

    // Pre-populate fields with default values so they're visible immediately
    ssh_ui_apply_field_defaults(app, INPUT_MODE_PORT);

    // Set initial input mode
    app->input_mode = INPUT_MODE_HOSTNAME;

    // Clear the screen and display SSH connection setup UI
    ui_manager_clear_screen();
    ui_manager_show_ssh_connection_setup(app);
}

void ssh_ui_progress_to_next_field(app_state_t* app) {
    if (!app) {
        return;
    }

    switch (app->input_mode) {
        case INPUT_MODE_HOSTNAME:
            app->input_mode = INPUT_MODE_PORT;
            // Initialize port field length from existing content
            app->connection_input.field_lengths.port = strlen(app->connection_input.port_str);
            break;

        case INPUT_MODE_PORT:
            app->input_mode = INPUT_MODE_USERNAME;
            // Initialize username field length from existing content
            app->connection_input.field_lengths.username = strlen(app->connection_input.username);
            break;

        case INPUT_MODE_USERNAME:
            // Skip password input and attempt connection directly
            // Authentication will be handled dynamically during connection

            // Change mode to normal before attempting connection to prevent
            // the username prompt from reappearing
            app->input_mode = INPUT_MODE_NORMAL;

            ssh_ui_attempt_connection(app);
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

        case INPUT_MODE_PORT:
            ssh_ui_apply_field_defaults(app, INPUT_MODE_PORT);
            ssh_ui_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;

        case INPUT_MODE_USERNAME:
            ssh_ui_apply_field_defaults(app, INPUT_MODE_USERNAME);
            ssh_ui_progress_to_next_field(app);
            return APP_RESULT_CONTINUE;

        default:
            // Not an SSH field - return error to indicate it's not handled here
            return APP_RESULT_ERROR;
    }
}

app_result_t ssh_ui_handle_auth_submit(app_state_t* app) {
    if (!app) {
        return APP_RESULT_ERROR;
    }

    if (app->input_mode != INPUT_MODE_AUTH_PROMPT) {
        return APP_RESULT_ERROR;
    }

    // Submit the authentication response to SSH manager
    bool success = ssh_manager_submit_auth_response(app, app->connection_input.auth_response);

    if (success) {
        // Clear the auth response buffer for security
        memset(app->connection_input.auth_response, 0, sizeof(app->connection_input.auth_response));
        app->connection_input.field_lengths.auth_response = 0;

        // Show feedback that response was submitted
        term_input_string("\r\n\x1b[90mResponse submitted...\x1b[0m\r\n");

        // Return to normal mode - SSH thread will generate new events
        app->input_mode = INPUT_MODE_NORMAL;

        return APP_RESULT_SUCCESS;
    } else {
        ui_manager_show_validation_error("Failed to submit authentication response");
        return APP_RESULT_RETRY;
    }
}

app_result_t ssh_ui_handle_disconnect_prompt(app_state_t* app, const char* input) {
    if (!app || !input) {
        return APP_RESULT_ERROR;
    }

    if (app->input_mode != INPUT_MODE_DISCONNECT_PROMPT) {
        return APP_RESULT_CONTINUE;
    }

    if (*input == '\r' || *input == '\n') {
        term_input_string("\r\n");
        if (!app->connection_succeeded) {
            // Connection failed — restart SSH input sequence to try again
            ssh_ui_start_connection_sequence(app);
            return APP_RESULT_RETRY;
        } else {
            // Session ended cleanly — return to main menu
            ssh_manager_cleanup(app);
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
    app->ssh_connecting = false;
    app->ssh_connected = false;
    app->connection_succeeded = false;
}

void ssh_ui_handle_connection_success(app_state_t* app) {
    if (!app) {
        return;
    }

    app->connection_succeeded = true;
    memset(&app->connection_input, 0, sizeof(connection_input_t));
}

app_result_t ssh_ui_attempt_connection(app_state_t* app) {
    if (!app) {
        return APP_RESULT_ERROR;
    }

    // Validate required fields (hostname, username, port)
    if (app->connection_input.field_lengths.hostname == 0) {
        ui_manager_show_validation_error("Hostname cannot be empty!");
        return APP_RESULT_RETRY;
    }

    if (app->connection_input.field_lengths.username == 0) {
        ui_manager_show_validation_error("Username cannot be empty!");
        return APP_RESULT_RETRY;
    }

    // Parse port with default fallback
    int port = atoi(app->connection_input.port_str);
    if (port <= 0 || port > 65535) {
        port = 22;
    }

    // Attempt connection via SSH manager (password will be prompted during authentication)
    return ssh_manager_connect_negotiate(app,
                                        app->connection_input.hostname,
                                        port,
                                        app->connection_input.username)
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
