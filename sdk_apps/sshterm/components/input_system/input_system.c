#include "input_system.h"
#include "../ui_manager/ui_manager.h"
#include "../../common/types.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
                .prompt = app->auth_prompt_text,
                .default_value = NULL,
                .is_password = !app->auth_prompt_echo,
                .numeric_only = false
            };
            break;
        default:
            break;
    }

    // Clamp cursor to valid range in case of stale state
    if (field.buffer) {
        if (app->input_cursor_pos < 0) {
            app->input_cursor_pos = 0;
        } else if (app->input_cursor_pos > field.length) {
            app->input_cursor_pos = field.length;
        }
        field.cursor_pos = app->input_cursor_pos;
    }

    return field;
}

void input_system_handle_char(app_state_t* app, char ch) {
    // Handle backspace / delete-left
    if (ch == '\b' || ch == 127) {
        input_field_t field = input_system_get_current_field(app);
        if (!field.buffer || app->input_cursor_pos <= 0) {
            return;
        }
        int pos = app->input_cursor_pos;
        // Shift everything from pos onward one position left
        memmove(field.buffer + pos - 1, field.buffer + pos, field.length - pos + 1);
        app->input_cursor_pos--;
        ui_manager_display_current_prompt(app);
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
        if ((size_t)field.length >= field.max_length) {
            return;
        }

        int pos = app->input_cursor_pos;
        // Open a slot at pos by shifting everything from pos rightward
        memmove(field.buffer + pos + 1, field.buffer + pos, field.length - pos + 1);
        field.buffer[pos] = ch;
        app->input_cursor_pos++;
        ui_manager_display_current_prompt(app);
    }
}

void input_system_handle_nav_key(app_state_t* app, int keycode) {
    if (!app) {
        return;
    }

    input_field_t field = input_system_get_current_field(app);
    if (!field.buffer) {
        return;
    }

    int pos = app->input_cursor_pos;

    if (keycode == SDLK_LEFT) {
        if (pos > 0) {
            app->input_cursor_pos = pos - 1;
        }
    } else if (keycode == SDLK_RIGHT) {
        if (pos < field.length) {
            app->input_cursor_pos = pos + 1;
        }
    } else if (keycode == SDLK_HOME) {
        app->input_cursor_pos = 0;
    } else if (keycode == SDLK_END) {
        app->input_cursor_pos = field.length;
    }
}
