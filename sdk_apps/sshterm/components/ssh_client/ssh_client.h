/**
 * @file ssh_client.h
 * @brief Low-level SSH client interface
 *
 * This file provides the public interface for low-level SSH protocol operations
 * using wolfSSH. It handles the SSH connection, authentication, channel management,
 * and data transfer at the protocol level.
 */

#ifndef SSH_CLIENT_H
#define SSH_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include "../../common/types.h"

/**
 * Initialize SSH client
 * @param client SSH client structure to initialize
 * @return true on success, false on failure
 */
bool ssh_client_init(ssh_client_t* client);

/**
 * Start SSH connection
 * @param client SSH client structure
 * @param hostname Remote hostname or IP address
 * @param port Remote port (usually 22)
 * @param username Username for authentication
 * @param password Password for authentication
 * @return true if connection initiated successfully, false on immediate error
 */
bool ssh_client_connect_start(ssh_client_t* client, const char* hostname, int port,
                             const char* username, const char* password);

/**
 * Continue SSH connection process (blocking)
 * This will block until the SSH connection completes (success or error)
 * @param client SSH client structure
 * @return false when done (check state for success/error)
 */
bool ssh_client_connect_continue(ssh_client_t* client);

/**
 * Send data to SSH session (blocking)
 * @param client SSH client structure
 * @param data Data buffer to send
 * @param len Length of data to send
 * @return true on success, false on failure
 */
bool ssh_client_send(ssh_client_t* client, const char* data, size_t len);

/**
 * Receive data from SSH session (blocking)
 * @param client SSH client structure
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @return Number of bytes received, 0 if no data available, -1 on error, -2 on clean disconnect
 */
int ssh_client_receive(ssh_client_t* client, char* buffer, int buffer_size);

/**
 * Resize the PTY window (for terminal window size changes)
 * @param client SSH client structure
 * @param width New terminal width in characters
 * @param height New terminal height in characters
 * @return true on success, false on failure
 */
bool ssh_client_resize_pty(ssh_client_t* client, int width, int height);

/**
 * Send a signal to the remote process (e.g., "TERM", "KILL", "INT")
 * @param client SSH client structure
 * @param signal_name Signal name to send
 * @return true on success, false on failure
 */
bool ssh_client_send_signal(ssh_client_t* client, const char* signal_name);

/**
 * Get the exit status of the remote process
 * @param client SSH client structure
 * @param exit_status Pointer to store the exit status
 * @return true if exit status is available, false otherwise
 */
bool ssh_client_get_exit_status(ssh_client_t* client, int* exit_status);

/**
 * Check if SSH client is connected and ready
 * @param client SSH client structure
 * @return true if connected, false otherwise
 */
bool ssh_client_is_connected(ssh_client_t* client);

/**
 * Get the current state of the SSH client
 * @param client SSH client structure
 * @return Current SSH state
 */
ssh_state_t ssh_client_get_state(ssh_client_t* client);

/**
 * Get the last error message
 * @param client SSH client structure
 * @return Error message string (may be empty)
 */
const char* ssh_client_get_error(ssh_client_t* client);

/**
 * Get the socket file descriptor for the SSH connection
 * @param client SSH client structure
 * @return Socket file descriptor, or -1 if not connected
 */
int ssh_client_get_fd(ssh_client_t* client);

/**
 * Peek at available data without consuming it (for wolfSSH threading pattern)
 * @param client SSH client structure
 * @return true if data is available, false otherwise
 */
bool ssh_client_peek(ssh_client_t* client);

/**
 * Cleanup SSH client resources
 * @param client SSH client structure
 */
void ssh_client_cleanup(ssh_client_t* client);

// === AUTHENTICATION HELPER FUNCTIONS ===

/**
 * Auth event callback function type
 * @param prompt_text The authentication prompt text
 * @param echo_input Whether the input should be echoed
 * @param method_name The authentication method name
 */
typedef void (*auth_event_callback_t)(const char* prompt_text, bool echo_input, const char* method_name);

/**
 * Set the auth event callback function
 * @param callback Callback function to call when auth prompts are needed
 */
void ssh_client_set_auth_event_callback(auth_event_callback_t callback);

/**
 * Check if authentication input is needed from user
 * @return true if auth input is needed, false otherwise
 */
bool ssh_client_needs_auth_input(void);

/**
 * Get current authentication prompt text
 * @return Prompt text string (may be empty)
 */
const char* ssh_client_get_auth_prompt(void);

/**
 * Check if current authentication prompt should echo input
 * @return true if input should be echoed, false for hidden input
 */
bool ssh_client_auth_prompt_echo(void);

/**
 * Submit authentication response from user
 * @param response User's response to the authentication prompt
 */
void ssh_client_submit_auth_response(const char* response);

#endif // SSH_CLIENT_H
