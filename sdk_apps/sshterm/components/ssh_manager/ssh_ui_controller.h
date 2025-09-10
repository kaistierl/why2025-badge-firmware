/**
 * @file ssh_ui_controller.h
 * @brief SSH User Interface Controller
 *
 * This component handles all SSH-related UI flows, input processing,
 * and user interaction sequences. It separates UI concerns from pure
 * SSH connection management.
 */

#ifndef SSH_UI_CONTROLLER_H
#define SSH_UI_CONTROLLER_H

#include "../../common/types.h"
#include "../../common/app_state.h"

/**
 * @brief Start SSH connection input sequence
 *
 * Initializes the SSH connection setup UI sequence with default values.
 * This moves the application into SSH input mode.
 *
 * @param app Application state
 */
void ssh_ui_start_connection_sequence(app_state_t* app);

/**
 * @brief Progress to the next SSH input field
 *
 * Advances the input sequence to the next SSH parameter field
 * (hostname -> username -> port -> password -> connection attempt).
 *
 * @param app Application state
 */
void ssh_ui_progress_to_next_field(app_state_t* app);

/**
 * @brief Apply default values to current input field
 *
 * Sets appropriate default values for the specified input field if it's empty.
 * Common defaults: port "22", etc.
 *
 * @param app Application state
 * @param field_mode Input mode representing the field to apply defaults to
 */
void ssh_ui_apply_field_defaults(app_state_t* app, input_mode_t field_mode);

/**
 * @brief Handle SSH connection field submission
 *
 * Processes submission of SSH input fields and validates input.
 * Manages the progression through the connection input sequence.
 *
 * @param app Application state
 * @param field_mode The SSH input mode that was submitted
 * @return Result of the field submission
 */
app_result_t ssh_ui_handle_field_submit(app_state_t* app, input_mode_t field_mode);

/**
 * @brief Handle authentication prompt submission
 *
 * Processes submission of authentication prompts (password or keyboard-interactive).
 * Sends the user's response to the SSH authentication process.
 *
 * @param app Application state
 * @return Result of the authentication submission
 */
app_result_t ssh_ui_handle_auth_submit(app_state_t* app);

/**
 * @brief Handle disconnect/retry prompt input
 *
 * Processes user input in the disconnect/retry prompt screen.
 * Provides options to retry connection, return to menu, etc.
 *
 * @param app Application state
 * @param input User input string
 * @return APP_RESULT_CONTINUE to continue, APP_RESULT_RETRY to restart SSH,
 *         APP_RESULT_CANCEL to return to startup menu
 */
app_result_t ssh_ui_handle_disconnect_prompt(app_state_t* app, const char* input);

/**
 * @brief Clear connection input data and reset to normal mode
 *
 * Resets all connection input fields to empty state and sets input mode to normal.
 * Used when connection succeeds or when cleaning up after disconnect.
 *
 * @param app Application state
 */
void ssh_ui_clear_connection_input(app_state_t* app);

/**
 * @brief Handle successful SSH connection transition
 *
 * Transitions from SSH input mode to normal terminal mode after
 * successful connection. Clears input fields and updates UI state.
 *
 * @param app Application state
 */
void ssh_ui_handle_connection_success(app_state_t* app);

/**
 * @brief Attempt SSH connection with validation
 *
 * Validates all entered connection parameters and attempts to establish
 * the SSH connection using the SSH manager.
 *
 * @param app Application state with connection input filled
 * @return APP_RESULT_SUCCESS on success, APP_RESULT_ERROR on failure,
 *         APP_RESULT_RETRY on validation error
 */
app_result_t ssh_ui_attempt_connection(app_state_t* app);

/**
 * @brief Display disconnect prompt UI
 *
 * Shows the disconnect/retry options to the user after a connection
 * ends or fails.
 *
 * @param app Application state
 */
void ssh_ui_display_disconnect_prompt(app_state_t* app);

#endif // SSH_UI_CONTROLLER_H
