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
 * Start non-blocking SSH connection
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
 * Continue non-blocking SSH connection process
 * Call this repeatedly until connection completes (state becomes CONNECTED or ERROR)
 * @param client SSH client structure
 * @return true if should continue calling (still in progress), false if done (success or error)
 */
bool ssh_client_connect_continue(ssh_client_t* client);

/**
 * Send data to SSH session
 * @param client SSH client structure
 * @param data Data buffer to send
 * @param len Length of data to send
 * @return true on success, false on failure
 */
bool ssh_client_send(ssh_client_t* client, const char* data, size_t len);

/**
 * Receive data from SSH session (non-blocking)
 * @param client SSH client structure
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @return Number of bytes received, 0 if no data available, -1 on error, -2 on clean disconnect
 */
int ssh_client_receive(ssh_client_t* client, char* buffer, size_t buffer_size);

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
 * Cleanup SSH client resources
 * @param client SSH client structure
 */
void ssh_client_cleanup(ssh_client_t* client);

#endif // SSH_CLIENT_H
