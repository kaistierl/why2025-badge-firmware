/**
 * @file ssh_manager.h
 * @brief SSH connection management interface
 * 
 * This file provides the public interface for managing SSH connections,
 * including connection establishment, data transfer, and connection state
 * management. This component handles all SSH-related business logic.
 */

#ifndef SSH_MANAGER_H
#define SSH_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

// Include common types and app state
#include "../../common/types.h"
#include "../../common/app_state.h"

/**
 * @brief Initialize the SSH manager
 * 
 * Sets up any resources needed for SSH connection management.
 * 
 * @return true on successful initialization, false on failure
 */
bool ssh_manager_init(void);

/**
 * @brief Shutdown the SSH manager
 * 
 * Cleans up all SSH manager resources and state.
 */
void ssh_manager_shutdown(void);

// === CONNECTION MANAGEMENT ===

/**
 * @brief Establish SSH connection
 * 
 * Attempts to connect to the specified SSH server with given credentials.
 * Handles authentication and session setup.
 * 
 * @param app Application state
 * @param hostname SSH server hostname or IP address
 * @param port SSH server port number
 * @param username Username for authentication
 * @param password Password for authentication
 * @return true on successful connection, false on failure
 */
bool ssh_manager_connect(app_state_t* app, const char* hostname, int port, 
                        const char* username, const char* password);

/**
 * @brief Disconnect from SSH server
 * 
 * Cleanly closes the SSH connection and frees associated resources.
 * 
 * @param app Application state
 */
void ssh_manager_disconnect(app_state_t* app);

/**
 * @brief Check if SSH connection is active
 * 
 * @param app Application state
 * @return true if connected and ready for data transfer, false otherwise
 */
bool ssh_manager_is_connected(app_state_t* app);

/**
 * @brief Check if SSH connection is in progress
 * 
 * @param app Application state
 * @return true if connection attempt is in progress, false otherwise
 */
bool ssh_manager_is_connecting(app_state_t* app);

// === DATA TRANSFER ===

/**
 * @brief Send data to SSH session
 * 
 * Transmits data to the remote SSH session (terminal input).
 * 
 * @param app Application state
 * @param data Data buffer to send
 * @param len Length of data in bytes
 * @return true on successful transmission, false on failure
 */
bool ssh_manager_send_data(app_state_t* app, const char* data, size_t len);

/**
 * @brief Poll and read data from SSH session
 * 
 * Checks for incoming data from the SSH session and processes it.
 * This function should be called regularly in the main loop.
 * 
 * @param app Application state
 * @return true if data was received and processed, false otherwise
 */
bool ssh_manager_poll_and_read(app_state_t* app);

// === STATE MANAGEMENT ===

/**
 * @brief Get last error message
 * 
 * @param app Application state
 * @return Error message string, or empty string if no error
 */
const char* ssh_manager_get_error(app_state_t* app);

/**
 * @brief Cleanup SSH connection state
 * 
 * Performs emergency cleanup of SSH connection resources.
 * 
 * @param app Application state
 */
void ssh_manager_cleanup(app_state_t* app);

/**
 * @brief Clear connection input data
 * 
 * Resets all connection input fields to empty state.
 * 
 * @param app Application state
 */
void ssh_manager_clear_connection_input(app_state_t* app);

// === HIGH-LEVEL CONNECTION OPERATIONS ===

/**
 * @brief Attempt SSH connection using current input
 * 
 * Uses the connection parameters stored in app state to attempt
 * an SSH connection with validation.
 * 
 * @param app Application state with connection input filled
 * @return APP_RESULT_SUCCESS on success, APP_RESULT_ERROR on failure, 
 *         APP_RESULT_RETRY on validation error
 */
app_result_t ssh_manager_attempt_connection(app_state_t* app);

/**
 * @brief Progress to the next input field
 * 
 * Advances the input sequence to the next SSH parameter field.
 * 
 * @param app Application state
 */
void ssh_manager_progress_to_next_field(app_state_t* app);

/**
 * @brief Apply default values to input field
 * 
 * Sets default values for the specified input field if it's empty.
 * 
 * @param app Application state
 * @param field_mode Input mode representing the field to apply defaults to
 */
void ssh_manager_apply_field_defaults(app_state_t* app, input_mode_t field_mode);

/**
 * @brief Start SSH input sequence
 * 
 * Initializes the SSH connection setup UI sequence with default values.
 * 
 * @param app Application state
 */
void ssh_manager_start_ui_sequence(app_state_t* app);

/**
 * @brief Handle disconnect prompt input
 * 
 * Processes user input in the disconnect/retry prompt screen.
 * 
 * @param app Application state
 * @param input User input string
 * @return APP_RESULT_CONTINUE to continue, APP_RESULT_RETRY to restart SSH,
 *         APP_RESULT_CANCEL to return to startup menu
 */
app_result_t ssh_manager_handle_disconnect_prompt(app_state_t* app, const char* input);

/**
 * @brief Handle SSH field submission
 * 
 * Processes submission of SSH input fields (hostname, username, port, password).
 * 
 * @param app Application state
 * @param field_mode The SSH input mode that was submitted
 * @return Result of the field submission
 */
app_result_t ssh_manager_handle_field_submit(app_state_t* app, input_mode_t field_mode);

#endif // SSH_MANAGER_H
