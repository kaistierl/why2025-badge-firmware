#include "input_system.h"
#include "../ssh_manager/ssh_manager.h"
#include "../ui_manager/ui_manager.h"
#include "../../common/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Get current input field configuration
input_field_t input_system_get_current_field(app_state_t* app) {
    input_field_t field = {0};

    switch (app->input_mode) {
        case INPUT_MODE_STARTUP_CHOICE:
            field = (input_field_t){
                .buffer = app->connection_input.startup_choice,
                .length = (int)strlen(app->connection_input.startup_choice),
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
                .length = (int)strlen(app->connection_input.hostname),
                .max_length = sizeof(app->connection_input.hostname) - 1,
                .prompt = "Hostname: ",
                .default_value = NULL,
                .is_password = false,
                .numeric_only = false
            };
            break;
        case INPUT_MODE_USERNAME:
            field = (input_field_t){
                .buffer = app->connection_input.username,
                .length = (int)strlen(app->connection_input.username),
                .max_length = sizeof(app->connection_input.username) - 1,
                .prompt = "Username: ",
                .default_value = NULL,
                .is_password = false,
                .numeric_only = false
            };
            break;
        case INPUT_MODE_PORT:
            field = (input_field_t){
                .buffer = app->connection_input.port_str,
                .length = (int)strlen(app->connection_input.port_str),
                .max_length = sizeof(app->connection_input.port_str) - 1,
                .prompt = "Port: ",
                .default_value = "22",
                .is_password = false,
                .numeric_only = true
            };
            break;
        case INPUT_MODE_AUTH_PROMPT:
            field = (input_field_t){
                .buffer = app->connection_input.auth_response,
                .length = (int)strlen(app->connection_input.auth_response),
                .max_length = sizeof(app->connection_input.auth_response) - 1,
                .prompt = ssh_manager_get_auth_prompt(),
                .default_value = NULL,
                .is_password = !ssh_manager_auth_prompt_echo(),
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
        if (field.length > 0) {
            field.buffer[field.length - 1] = '\0';
            ui_manager_display_current_prompt(app);
        }
        return;
    }

    // Handle printable characters
    if (ch >= 32 && ch <= 126) {
        input_field_t field = input_system_get_current_field(app);
        if (!field.buffer) {
            return;
        }

        // Check numeric constraint
        if (field.numeric_only && (ch < '0' || ch > '9')) {
            return;
        }

        // Check buffer space
        if ((size_t)field.length < field.max_length) {
            field.buffer[field.length] = ch;
            field.buffer[field.length + 1] = '\0';
            ui_manager_display_current_prompt(app);
        }
    }
}
