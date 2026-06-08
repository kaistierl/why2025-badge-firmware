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
#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>

// SSH thread command types
typedef enum {
    SSH_CMD_CONNECT,
    SSH_CMD_SEND_RAW_INPUT,
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
            char input_data[SSH_RAW_INPUT_LEN];
            size_t input_len;
        } send_raw_input;
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
            char prompt_text[SSH_AUTH_PROMPT_LEN];
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

// Thread arguments structure
typedef struct {
    ssh_client_t* ssh_client;
    struct ssh_thread_manager* manager;  // Forward reference
    bool quit;  // Protected by manager->state_mutex
} ssh_thread_args_t;

// SSH thread manager state
typedef struct ssh_thread_manager {
    pid_t ssh_thread_id;
    bool thread_running;
    bool shutdown_requested;

    // Thread coordination flags (protected by state_mutex)
    bool shutdown_complete;   // Set by worker thread before exit
    bool worker_ready;        // Set when worker thread finishes initialization

    // Thread handles and state for I/O threads
    pid_t read_input_thread_id;
    pid_t read_peer_thread_id;
    ssh_thread_args_t thread_args;

    // Thread state tracking for cooperative termination (protected by state_mutex)
    bool input_thread_active;
    bool peer_thread_active;
    bool input_thread_complete;
    bool peer_thread_complete;

    // Thread synchronization
    SDL_Mutex* cmd_queue_mutex;    // Protects command queue operations
    SDL_Mutex* event_queue_mutex;  // Protects event queue operations
    SDL_Mutex* state_mutex;        // Protects thread state variables
    SDL_Condition* state_cond;     // Signaled on any state flag change
    SDL_Condition* cmd_cond;       // Signaled when a command is enqueued

    // Thread communication queues — access protected by cmd_queue_mutex/event_queue_mutex
    ssh_cmd_t cmd_queue[SSH_QUEUE_SIZE];
    int cmd_queue_head;
    int cmd_queue_tail;
    int cmd_queue_count;

    ssh_event_t event_queue[SSH_QUEUE_SIZE];
    int event_queue_head;
    int event_queue_tail;
    int event_queue_count;

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
bool ssh_thread_send_raw_input(ssh_thread_manager_t* manager, const char* input, size_t len);
void ssh_thread_disconnect(ssh_thread_manager_t* manager);

// Check if SSH thread is running
bool ssh_thread_is_running(ssh_thread_manager_t* manager);

// Submit auth response to the SSH client owned by this manager (thread-safe)
void ssh_thread_submit_auth_response(ssh_thread_manager_t* manager, const char* response);

#endif /* SSH_THREAD_H */
