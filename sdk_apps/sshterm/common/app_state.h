/**
 * @file app_state.h
 * @brief Application state structure definition
 * 
 * This file defines the central application state structure that holds
 * all runtime state information for the SSH Terminal application.
 */

#ifndef APP_STATE_COMMON_H
#define APP_STATE_COMMON_H

#include "types.h"

/**
 * @brief Central application state structure
 * 
 * Contains all runtime state information including SSH connection status,
 * current input mode, and connection parameters. This structure is passed
 * between components to maintain consistent application state.
 */
struct app_state {
    bool ssh_connected;         /**< Whether SSH connection is active */
    bool ssh_connecting;        /**< Whether SSH connection is in progress */
    bool had_ssh_session;       /**< Whether user has attempted SSH connection */
    ssh_client_t ssh_client;    /**< SSH client connection state and handles */
    input_mode_t input_mode;    /**< Current user input mode */
    connection_input_t connection_input; /**< User-entered connection parameters */
};

#endif // APP_STATE_COMMON_H
