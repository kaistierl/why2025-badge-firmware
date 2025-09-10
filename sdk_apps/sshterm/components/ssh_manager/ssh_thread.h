/* ssh_thread.h - Threaded SSH I/O manager for BadgeVMS
 * Handles SSH operations in a separate thread to prevent UI blocking
 */

#ifndef SSH_THREAD_H
#define SSH_THREAD_H

#include "../ssh_client/ssh_client.h"
#include "ssh_config.h"
#include "../../common/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// SSH thread command types
typedef enum {
    SSH_CMD_CONNECT,
    SSH_CMD_SEND_DATA,
    SSH_CMD_DISCONNECT,
    SSH_CMD_SHUTDOWN
} ssh_cmd_type_t;

// SSH thread event types (from SSH thread to main thread)
typedef enum {
    SSH_EVENT_CONNECTED,
    SSH_EVENT_CONNECTION_FAILED,
    SSH_EVENT_DATA_RECEIVED,
    SSH_EVENT_AUTH_PROMPT,
    SSH_EVENT_DISCONNECTED,
    SSH_EVENT_ERROR
} ssh_event_type_t;

// Command sent to SSH thread
typedef struct {
    ssh_cmd_type_t type;
    union {
        struct {
            char hostname[SSH_MAX_HOSTNAME_LEN];
            char username[SSH_MAX_USERNAME_LEN];
            char password[SSH_MAX_PASSWORD_LEN];
            int port;
        } connect;
        struct {
            char data[SSH_DATA_BUFFER_SIZE];
            size_t len;
        } send_data;
        struct {
            char message[SSH_MAX_ERROR_MSG_LEN];
        } error;
    };
} ssh_cmd_t;

// Event sent from SSH thread to main thread
typedef struct {
    ssh_event_type_t type;
    union {
        struct {
            char data[SSH_DATA_BUFFER_SIZE];
            size_t len;
        } data_received;
        struct {
            char method_name[32];
            char prompt_text[512];
            bool echo_input;
        } auth_prompt;
        struct {
            char message[SSH_MAX_ERROR_MSG_LEN];
        } error;
        struct {
            char hostname[SSH_MAX_HOSTNAME_LEN];
            char username[SSH_MAX_USERNAME_LEN];
        } connected;
    };
} ssh_event_t;

// SSH thread manager state
typedef struct {
    pid_t ssh_thread_id;
    bool thread_running;
    bool shutdown_requested;

    // Thread communication queues (simulated with simple arrays for now)
    // In a full implementation, these would be proper thread-safe queues
    ssh_cmd_t cmd_queue[SSH_QUEUE_SIZE];
    volatile int cmd_queue_head;
    volatile int cmd_queue_tail;
    volatile int cmd_queue_count;

    ssh_event_t event_queue[SSH_QUEUE_SIZE];
    volatile int event_queue_head;
    volatile int event_queue_tail;
    volatile int event_queue_count;

    // SSH client instance (used by SSH thread)
    ssh_client_t ssh_client;
} ssh_thread_manager_t;

// Initialize SSH thread manager
bool ssh_thread_init(ssh_thread_manager_t* manager);

// Shutdown SSH thread manager
void ssh_thread_shutdown(ssh_thread_manager_t* manager);

// Send command to SSH thread (non-blocking)
bool ssh_thread_send_command(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd);

// Poll for events from SSH thread (non-blocking)
bool ssh_thread_poll_event(ssh_thread_manager_t* manager, ssh_event_t* event);

// Convenience functions for common operations
bool ssh_thread_connect(ssh_thread_manager_t* manager, const char* hostname, int port,
                       const char* username, const char* password);
bool ssh_thread_send_data(ssh_thread_manager_t* manager, const char* data, size_t len);
void ssh_thread_disconnect(ssh_thread_manager_t* manager);

// Check if SSH thread is running
bool ssh_thread_is_running(ssh_thread_manager_t* manager);

// Check if SSH is connected
bool ssh_thread_is_connected(ssh_thread_manager_t* manager);

#endif /* SSH_THREAD_H */
