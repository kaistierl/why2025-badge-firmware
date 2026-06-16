/**
 * @file ssh_config.h
 * @brief SSH subsystem configuration constants
 * 
 * This file centralizes all configuration constants used across the SSH
 * subsystem components for consistency and maintainability.
 */

#ifndef SSH_CONFIG_H
#define SSH_CONFIG_H

// === BUFFER SIZE CONFIGURATION ===

/** Maximum hostname length */
#define SSH_MAX_HOSTNAME_LEN    256

/** Raw terminal input command payload */
#define SSH_RAW_INPUT_LEN       256

/** Authentication prompt text from server */
#define SSH_AUTH_PROMPT_LEN     512

/** Maximum username length */
#define SSH_MAX_USERNAME_LEN    64

/** Maximum password length */
#define SSH_MAX_PASSWORD_LEN    256

/** Maximum error message length */
#define SSH_MAX_ERROR_MSG_LEN   256

/** SSH data buffer size for I/O operations */
#define SSH_DATA_BUFFER_SIZE    4096

// === QUEUE CONFIGURATION ===

/** Command queue depth (lifecycle + keyboard; never accumulates) */
#define SSH_CMD_QUEUE_SIZE      4

/** Event queue depth (SSH data burst buffer before backpressure kicks in) */
#define SSH_EVENT_QUEUE_SIZE    8

// === CONNECTION CONFIGURATION ===

/** Default SSH port */
#define SSH_DEFAULT_PORT        22

/** Maximum connection attempts */
#define SSH_MAX_CONNECT_ATTEMPTS 3

/** Connection timeout in seconds */
#define SSH_CONNECT_TIMEOUT_SEC  30

// === THREADING CONFIGURATION ===

/** SSH main thread stack size */
#define SSH_MAIN_THREAD_STACK   32768

/** SSH I/O thread stack size */
#define SSH_IO_THREAD_STACK     16384


#endif // SSH_CONFIG_H
