/**
 * @file input_system.h
 * @brief Input system interface
 * 
 * This file provides the public interface for handling user input processing,
 * field management, and input validation across different application modes.
 */

#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../../common/app_state.h"

/**
 * @brief Initialize the input system
 * 
 * Sets up any resources needed for input processing.
 * 
 * @return true on successful initialization, false on failure
 */
bool input_system_init(void);

/**
 * @brief Shutdown the input system
 * 
 * Cleans up all input system resources and state.
 */
void input_system_shutdown(void);

// === INPUT PROCESSING ===

/**
 * @brief Handle character input
 * 
 * Processes a single character of user input, including backspace handling
 * and field-specific validation (e.g., numeric-only fields).
 * 
 * @param app Application state
 * @param ch Character to process
 */
void input_system_handle_char(app_state_t* app, char ch);

/**
 * @brief Handle Enter key press
 * 
 * Processes Enter key input, applying default values if needed and
 * triggering field submission logic.
 * 
 * @param app Application state
 */
void input_system_handle_enter(app_state_t* app);

/**
 * @brief Handle Escape key press
 * 
 * Processes Escape key input for cancelling current operations.
 * 
 * @param app Application state
 */
void input_system_handle_escape_key(app_state_t* app);

/**
 * @brief Handle terminal output
 * 
 * Processes terminal output data for display.
 * 
 * @param app Application state
 * @param data Output data buffer
 * @param len Length of output data
 */
void input_system_handle_terminal_output(app_state_t* app, const uint8_t* data, size_t len);

// === UI MANAGEMENT ===

/**
 * @brief Display the current input prompt
 * 
 * Shows the appropriate prompt for the current input mode.
 * 
 * @param app Application state
 */
void input_system_display_prompt(app_state_t* app);

/**
 * @brief Show startup menu
 * 
 * Displays the application startup menu.
 * 
 * @param app Application state
 */
void input_system_show_startup_menu(app_state_t* app);

// === STATE MANAGEMENT ===

/**
 * @brief Clear input system state
 * 
 * Resets all input-related state to initial values.
 * 
 * @param app Application state
 */
void input_system_clear_state(app_state_t* app);

// === HELPER FUNCTIONS ===

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

#endif // INPUT_SYSTEM_H
