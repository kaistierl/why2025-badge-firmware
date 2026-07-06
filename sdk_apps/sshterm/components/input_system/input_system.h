/**
 * @file input_system.h
 * @brief Input field management
 *
 * Pure data module: maps the current input mode to a field descriptor
 * and accumulates characters into it. All routing (Enter, Escape,
 * terminal-output forwarding) is handled by app_controller.
 */

#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../../common/app_state.h"

/**
 * @brief Get current input field configuration
 *
 * Returns the input field configuration for the current input mode,
 * including buffer pointers, validation rules, and display settings.
 *
 * @param app Application state
 * @return Input field configuration structure
 */
input_field_t input_system_get_current_field(app_state_t* app);

/**
 * @brief Handle character input
 *
 * Processes a single printable character or backspace into the active
 * field buffer and refreshes the prompt display.
 *
 * @param app Application state
 * @param ch Character to process
 */
void input_system_handle_char(app_state_t* app, char ch);

/**
 * @brief Handle cursor navigation key in an input field
 *
 * Moves app->input_cursor_pos in response to Left/Right/Home/End.
 * Caller is responsible for refreshing the prompt display afterwards.
 *
 * @param app Application state
 * @param keycode SDL_Keycode value (SDLK_LEFT / SDLK_RIGHT / SDLK_HOME / SDLK_END)
 */
void input_system_handle_nav_key(app_state_t* app, int keycode);

#endif // INPUT_SYSTEM_H
