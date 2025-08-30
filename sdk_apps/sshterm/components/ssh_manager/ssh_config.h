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

/** Maximum username length */
#define SSH_MAX_USERNAME_LEN    64

/** Maximum password length */
#define SSH_MAX_PASSWORD_LEN    256

/** Maximum error message length */
#define SSH_MAX_ERROR_MSG_LEN   256

/** SSH data buffer size for I/O operations */
#define SSH_DATA_BUFFER_SIZE    4096

// === QUEUE CONFIGURATION ===

/** Size of command/event queues for thread communication */
#define SSH_QUEUE_SIZE          16

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

/** Thread startup delay in microseconds */
#define SSH_THREAD_STARTUP_DELAY 50000

/** Thread shutdown wait time in microseconds */
#define SSH_THREAD_SHUTDOWN_WAIT 100000

/** Thread polling interval in microseconds */
#define SSH_THREAD_POLL_INTERVAL 50000

#endif // SSH_CONFIG_H
