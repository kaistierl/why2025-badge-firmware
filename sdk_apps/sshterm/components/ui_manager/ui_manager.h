/**
 * @file ui_manager.h
 * @brief User interface management interface
 * 
 * This file provides the public interface for all UI formatting, screen layouts,
 * and terminal output management. Business logic modules should delegate all
 * UI presentation concerns to this component.
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdbool.h>
#include "../../common/app_state.h"

/**
 * @brief Initialize the UI manager
 * 
 * Sets up any resources needed for UI management.
 * 
 * @return true on successful initialization, false on failure
 */
bool ui_manager_init(void);

/**
 * @brief Shutdown the UI manager
 * 
 * Cleans up all UI manager resources and state.
 */
void ui_manager_shutdown(void);

// === MAIN UI SCREENS ===

/**
 * @brief Display the application startup menu
 * 
 * Shows the main menu allowing user to choose between SSH mode and test mode.
 * 
 * @param app Application state
 */
void ui_manager_show_startup_menu(app_state_t* app);

/**
 * @brief Display the SSH connection setup screen
 * 
 * Shows the SSH parameter input interface (hostname, username, port, password).
 * 
 * @param app Application state
 */
void ui_manager_show_ssh_connection_setup(app_state_t* app);

/**
 * @brief Display the current input prompt based on app state
 * 
 * Shows the appropriate input prompt for the current application mode.
 * 
 * @param app Application state
 */
void ui_manager_display_current_prompt(app_state_t* app);

// === STATUS AND FEEDBACK MESSAGES ===

/**
 * @brief Show connection cancelled message
 */
void ui_manager_show_connection_cancelled_message(void);

/**
 * @brief Show test mode activation message
 */
void ui_manager_show_test_mode_message(void);

/**
 * @brief Show help/usage message
 */
void ui_manager_show_help_message(void);

// === SSH CONNECTION STATUS MESSAGES ===

/**
 * @brief Show connecting status message
 * 
 * Displays the connecting message with immediate rendering to ensure
 * it appears on screen right away.
 */
void ui_manager_show_connecting_message(void);

/**
 * @brief Show SSH connection error message
 * 
 * @param error Error message string to display
 */
void ui_manager_show_connection_error(const char* error);

/**
 * @brief Show input validation error message
 * 
 * @param error Validation error message to display
 */
void ui_manager_show_validation_error(const char* error);

/**
 * @brief Show successful SSH connection message
 * 
 * @param hostname Connected hostname
 * @param username Connected username
 */
void ui_manager_show_connection_success(const char* hostname, const char* username);

// === INPUT FIELD MANAGEMENT ===

/**
 * @brief Display prompt for a specific input field
 * 
 * Shows the prompt and current content for an input field, handling
 * password masking and other field-specific display requirements.
 * 
 * @param field Input field configuration and data
 */
void ui_manager_display_field_prompt(input_field_t field);

// === UTILITY FUNCTIONS ===

/**
 * @brief Clear the terminal screen
 */
void ui_manager_clear_screen(void);

/**
 * @brief Show a formatted header with title
 * 
 * @param title Header title text
 */
void ui_manager_show_header(const char* title);

#endif // UI_MANAGER_H
