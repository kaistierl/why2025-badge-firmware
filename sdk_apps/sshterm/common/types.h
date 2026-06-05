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
#include <stdint.h>

/**
 * @brief Input modes for different application states
 */
typedef enum {
    INPUT_MODE_NORMAL,          /**< Normal terminal input mode */
    INPUT_MODE_STARTUP_CHOICE,  /**< Startup menu selection mode */
    INPUT_MODE_HOSTNAME,        /**< SSH hostname input mode */
    INPUT_MODE_USERNAME,        /**< SSH username input mode */
    INPUT_MODE_PORT,            /**< SSH port input mode */
    INPUT_MODE_AUTH_PROMPT,     /**< Dynamic authentication prompt mode */
    INPUT_MODE_DISCONNECT_PROMPT /**< Disconnect/retry prompt mode */
} input_mode_t;

/**
 * @brief Authentication methods supported by SSH
 */
typedef enum {
    AUTH_METHOD_PASSWORD,           /**< Traditional password authentication */
    AUTH_METHOD_KEYBOARD_INTERACTIVE /**< Keyboard-interactive authentication */
} auth_method_t;

/**
 * @brief Authentication prompt state for dynamic prompts
 *
 * Used to manage keyboard-interactive authentication prompts
 * and facilitate communication between SSH thread and main thread.
 */
typedef struct {
    auth_method_t method;           /**< Current authentication method */
    char prompt_text[512];          /**< Current prompt text from server */
    bool echo_input;                /**< Whether input should be echoed */
    bool response_ready;            /**< Flag indicating response is ready */
    char current_response[512];     /**< Current response buffer */
} auth_prompt_t;

/**
 * @brief Connection input data structure
 *
 * Contains all SSH connection parameters entered by the user.
 * Field lengths are computed via strlen() rather than maintained separately.
 */
typedef struct {
    char hostname[256];         /**< SSH server hostname or IP address */
    char username[256];         /**< SSH username for authentication */
    char port_str[16];          /**< SSH port as string (default: "22") */
    char startup_choice[16];    /**< User's startup menu choice */
    char auth_response[512];    /**< Dynamic authentication response */
} connection_input_t;

/**
 * @brief Input field abstraction for unified handling
 *
 * Provides a generic interface for handling different types of input fields
 * with consistent validation and display behavior.
 */
typedef struct {
    char* buffer;               /**< Pointer to the field's data buffer */
    int length;                 /**< Current length (strlen of buffer) */
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
 * @brief Machine-readable SSH error categories
 *
 * Stored alongside the human-readable error_msg so callers can branch on
 * error type without parsing strings.
 */
typedef enum {
    SSH_ERR_NONE = 0,
    SSH_ERR_INVALID_PARAM,
    SSH_ERR_MUTEX_FAILED,
    SSH_ERR_WOLFSSL_INIT,
    SSH_ERR_RNG_FAILED,
    SSH_ERR_WOLFSSH_INIT,
    SSH_ERR_CONTEXT_FAILED,
    SSH_ERR_SESSION_FAILED,
    SSH_ERR_SOCKET_FAILED,
    SSH_ERR_HANDSHAKE_FAILED,
    SSH_ERR_SEND_FAILED,
    SSH_ERR_RECV_FAILED,
} ssh_error_code_t;

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
 * Now includes secure authentication state management and thread safety.
 */
typedef struct ssh_client {
    char hostname[256];         /**< SSH server hostname (stored copy) */
    int port;                   /**< SSH server port number */
    const char* username;       /**< SSH username (reference) */
    ssh_state_t state;          /**< Current connection state */
    char error_msg[256];        /**< Last error message if any */
    ssh_error_code_t last_error_code; /**< Machine-readable error category */
    auth_prompt_t auth_prompt;  /**< Current authentication prompt state */

    // Private implementation details
    void* ctx;                  /**< WOLFSSH_CTX* (opaque pointer) */
    void* ssh;                  /**< WOLFSSH* (opaque pointer) */
    int socket_fd;              /**< Network socket file descriptor */

    // NEW: Authentication state (replaces globals)
    struct {
        char stored_password[512];      /**< Secure local copy of password */
        char stored_username[256];      /**< Secure local copy of username */
        char auth_response_buffer[512]; /**< Thread-safe response buffer */
        volatile bool auth_response_ready;  /**< Flag for response availability */
        volatile bool auth_input_needed;    /**< Flag for input requirement */
        volatile bool auth_in_progress;     /**< Flag for auth process state */
        int password_retry_count;           /**< Password retry counter */
        void* auth_event_callback;          /**< auth_event_callback_t function pointer */
        void* auth_state_mutex;             /**< SDL_Mutex* for thread safety */
    } auth_state;

    // NEW: Memory safety enhancements
    bool is_initialized;        /**< Initialization sentinel */
    uint32_t magic_number;      /**< Corruption detection (0xDEADBEEF when valid) */
} ssh_client_t;

#endif // COMMON_TYPES_H
