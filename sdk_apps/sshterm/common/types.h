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
    INPUT_MODE_AUTH_PROMPT,     /**< Dynamic authentication prompt mode */
    INPUT_MODE_DISCONNECT_PROMPT /**< Disconnect/retry prompt mode */
} input_mode_t;

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
    int cursor_pos;             /**< Cursor byte offset within buffer (0 = before first char) */
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

#endif // COMMON_TYPES_H
