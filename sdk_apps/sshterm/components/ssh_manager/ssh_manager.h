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
 * @brief Establish SSH connection with dynamic authentication
 *
 * Attempts to connect to the specified SSH server and negotiate authentication
 * dynamically. Authentication prompts will be handled via events.
 *
 * @param app Application state
 * @param hostname SSH server hostname or IP address
 * @param port SSH server port number
 * @param username Username for authentication
 * @return true on successful connection start, false on failure
 */
bool ssh_manager_connect_negotiate(app_state_t* app, const char* hostname, int port,
                                  const char* username);

/**
 * @brief Submit authentication response
 *
 * Sends user's authentication response (password or keyboard-interactive prompt)
 * to the SSH authentication process.
 *
 * @param app Application state
 * @param response User's response to authentication prompt
 * @return true on successful submission, false on failure
 */
bool ssh_manager_submit_auth_response(app_state_t* app, const char* response);

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
 * @brief Cleanup SSH connection state
 *
 * Performs emergency cleanup of SSH connection resources.
 *
 * @param app Application state
 */
void ssh_manager_cleanup(app_state_t* app);

#endif // SSH_MANAGER_H
