/**
 * @file app_controller.h
 * @brief Application controller interface
 * 
 * This file provides the public interface for the main application controller,
 * which is responsible for SDL system lifecycle management, event processing,
 * and high-level application flow coordination.
 */

#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "../../common/types.h"
#include "../../common/app_state.h"

/**
 * @brief Forward declaration for SDL event structure
 */
typedef union SDL_Event SDL_Event;

/**
 * @brief Opaque application controller structure
 */
typedef struct app_controller app_controller_t;

// === PRIMARY PUBLIC INTERFACE ===

/**
 * Initialize the application controller
 * @param controller Pointer to receive the controller instance
 * @return true on success, false on failure
 */
bool app_controller_init(app_controller_t** controller);

/**
 * Run the application main loop
 * @param controller The controller instance
 * @return true on successful completion, false on error
 */
bool app_controller_run(app_controller_t* controller);

/**
 * Request graceful shutdown of the application
 * @param controller The controller instance
 */
void app_controller_request_shutdown(app_controller_t* controller);

/**
 * Shutdown and cleanup the application controller
 * @param controller The controller instance
 */
void app_controller_shutdown(app_controller_t* controller);

// === UTILITY FUNCTIONS ===

/**
 * Create default application state
 * @return Initialized app_state_t structure
 */
app_state_t app_controller_create_default_state(void);

/**
 * Return to the startup menu
 * @param app Application state
 */
void app_controller_return_to_startup(app_state_t* app);

/**
 * Handle startup menu choice selection
 * @param app Application state
 * @param choice The menu choice (1 for SSH, 2 for test mode)
 */
void app_controller_handle_startup_choice(app_state_t* app, int choice);

/**
 * Handle test mode input when not connected to SSH
 * @param data Input data buffer
 * @param len Length of input data
 */
void app_controller_handle_test_mode_input(const char *data, size_t len);

/**
 * Handle field input submission (Enter key in input fields)
 * @param app Application state
 * @param field_mode The input mode/field being submitted
 */
void app_controller_handle_field_submit(app_state_t* app, input_mode_t field_mode);

/**
 * Handle disconnect prompt input
 * @param app Application state
 * @param input User input string
 */
void app_controller_handle_disconnect_prompt(app_state_t* app, const char* input);

/**
 * Handle escape key press (cancel current operation)
 * @param app Application state
 */
void app_controller_handle_escape_key(app_state_t* app);

/**
 * Handle terminal data output and routing
 * @param app Application state
 * @param data Terminal output data
 * @param len Length of data
 */
void app_controller_handle_terminal_output(app_state_t* app, const uint8_t* data, size_t len);

/**
 * Display current input prompt through UI manager
 * @param app Application state
 */
void app_controller_display_current_prompt(app_state_t* app);

/**
 * Show startup menu through UI manager
 * @param app Application state
 */
void app_controller_show_startup_menu(app_state_t* app);

#endif // APP_CONTROLLER_H
