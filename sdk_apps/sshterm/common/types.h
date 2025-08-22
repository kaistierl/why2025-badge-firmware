/**
 * @file types.h
 * @brief Common type definitions and data structures for SSH Terminal application
 * 
 * This file contains all shared data structures, enums, and type definitions
 * used across the SSH Terminal application components.
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Input modes for different application states
 */
typedef enum {
    INPUT_MODE_NORMAL,          /**< Normal terminal input mode */
    INPUT_MODE_STARTUP_CHOICE,  /**< Startup menu selection mode */
    INPUT_MODE_HOSTNAME,        /**< SSH hostname input mode */
    INPUT_MODE_USERNAME,        /**< SSH username input mode */
    INPUT_MODE_PORT,            /**< SSH port input mode */
    INPUT_MODE_PASSWORD,        /**< SSH password input mode */
    INPUT_MODE_DISCONNECT_PROMPT /**< Disconnect/retry prompt mode */
} input_mode_t;

/**
 * @brief Connection input data structure
 * 
 * Contains all SSH connection parameters entered by the user.
 */
typedef struct {
    char hostname[256];         /**< SSH server hostname or IP address */
    char username[256];         /**< SSH username for authentication */
    char port_str[16];          /**< SSH port as string (default: "22") */
    char password[256];         /**< SSH password for authentication */
    char startup_choice[16];    /**< User's startup menu choice */
    
    /**
     * @brief Field length tracking for input validation
     */
    struct {
        int hostname;           /**< Current length of hostname field */
        int username;           /**< Current length of username field */
        int port;               /**< Current length of port field */
        int password;           /**< Current length of password field */
        int startup_choice;     /**< Current length of startup choice field */
    } field_lengths;
} connection_input_t;

/**
 * @brief Input field abstraction for unified handling
 * 
 * Provides a generic interface for handling different types of input fields
 * with consistent validation and display behavior.
 */
typedef struct {
    char* buffer;               /**< Pointer to the field's data buffer */
    int* length;                /**< Pointer to the field's current length */
    size_t max_length;          /**< Maximum allowed length for this field */
    const char* prompt;         /**< Prompt text to display for this field */
    const char* default_value;  /**< Default value to use if field is empty */
    bool is_password;           /**< Whether field should be masked during display */
    bool numeric_only;          /**< Whether field accepts only numeric input */
} input_field_t;

/**
 * @brief Forward declaration of application state structure
 */
typedef struct app_state app_state_t;

/**
 * @brief Application operation result codes
 */
typedef enum {
    APP_RESULT_SUCCESS,         /**< Operation completed successfully */
    APP_RESULT_ERROR,           /**< Operation failed with error */
    APP_RESULT_RETRY,           /**< Operation should be retried */
    APP_RESULT_CANCEL,          /**< Operation was cancelled by user */
    APP_RESULT_CONTINUE         /**< Operation should continue */
} app_result_t;

/**
 * @brief SSH client connection state
 */
typedef enum {
    SSH_STATE_DISCONNECTED,       /**< Not connected to any SSH server */
    SSH_STATE_SOCKET_CONNECTING,  /**< Creating TCP socket connection */
    SSH_STATE_SSH_HANDSHAKING,    /**< SSH protocol handshake in progress */
    SSH_STATE_AUTHENTICATING,     /**< Performing authentication handshake */
    SSH_STATE_CONNECTED,          /**< Successfully connected and authenticated */
    SSH_STATE_ERROR               /**< Connection failed or encountered error */
} ssh_state_t;

/**
 * @brief SSH client structure
 * 
 * Contains all data needed to manage an SSH connection including
 * connection parameters, state, and internal wolfSSH handles.
 */
typedef struct ssh_client {
    char hostname[256];         /**< SSH server hostname (stored copy) */
    int port;                   /**< SSH server port number */
    const char* username;       /**< SSH username (reference) */
    ssh_state_t state;          /**< Current connection state */
    char error_msg[256];        /**< Last error message if any */
    
    // Private implementation details
    void* ctx;                  /**< WOLFSSH_CTX* (opaque pointer) */
    void* ssh;                  /**< WOLFSSH* (opaque pointer) */
    int socket_fd;              /**< Network socket file descriptor */
} ssh_client_t;

#endif // COMMON_TYPES_H
