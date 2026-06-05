/* ssh_thread.c - Threaded SSH I/O manager implementation
 * Pure blocking I/O version without select() for BadgeVMS compatibility
 */

#include "ssh_thread.h"
#include "badgevms/process.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>

// Forward declarations
static void read_input_thread(void* arg);
static void read_peer_thread(void* arg);
static void ssh_thread_main(void* data);
static bool queue_put_cmd(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd);
static bool queue_get_cmd(ssh_thread_manager_t* manager, ssh_cmd_t* cmd);
static bool queue_peek_cmd_type(ssh_thread_manager_t* manager, ssh_cmd_type_t* type);
static bool queue_put_event(ssh_thread_manager_t* manager, const ssh_event_t* event);
static bool queue_get_event(ssh_thread_manager_t* manager, ssh_event_t* event);
static void ssh_auth_event_callback(ssh_client_t* client, const char* prompt_text, bool echo_input, const char* method_name);
static bool wait_for_io_threads(ssh_thread_manager_t* manager, Sint32 timeout_ms);

// Global singleton for auth event callback routing.
// Only one manager can be active at a time (single-connection constraint).
// Passing the manager through wolfSSH callbacks is not straightforward because
// the wolfSSH auth callback signature only provides authType, authData, and ctx,
// and ctx is already used for the ssh_client instance. A second indirection
// through a global is the simplest correct solution given this constraint.
static ssh_thread_manager_t* g_current_auth_manager = NULL;
static SDL_Mutex* g_auth_manager_mutex = NULL;

// Read input thread - handles keyboard input and sends to SSH
static void read_input_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;

    // Signal thread is active
    SDL_LockMutex(args->manager->state_mutex);
    args->manager->input_thread_active = true;
    SDL_UnlockMutex(args->manager->state_mutex);

    while (true) {
        // Check quit flag with mutex protection
        SDL_LockMutex(args->manager->state_mutex);
        bool should_quit = args->quit;
        bool manager_running = args->manager->thread_running;
        SDL_UnlockMutex(args->manager->state_mutex);

        if (should_quit || !manager_running) {
            break;
        }

        // Only dequeue SSH_CMD_SEND_RAW_INPUT; leave control commands for the main thread.
        ssh_cmd_type_t next_type;
        if (queue_peek_cmd_type(args->manager, &next_type) &&
            next_type == SSH_CMD_SEND_RAW_INPUT) {

            ssh_cmd_t cmd;
            if (queue_get_cmd(args->manager, &cmd)) {
                bool sent = ssh_client_send(args->ssh_client,
                                            cmd.send_raw_input.input_data,
                                            cmd.send_raw_input.input_len);
                if (!sent) {
                    printf("SSH Input Thread: Failed to send input: %s\n",
                           ssh_client_get_error(args->ssh_client));
                    ssh_event_t event = {.type = SSH_EVENT_ERROR};
                    strncpy(event.error.message, ssh_client_get_error(args->ssh_client),
                            sizeof(event.error.message) - 1);
                    queue_put_event(args->manager, &event);
                }
            }
        }

        // Small delay to prevent busy waiting
        usleep(SSH_THREAD_POLL_INTERVAL); // 50ms
    }

    // Signal thread completion
    SDL_LockMutex(args->manager->state_mutex);
    args->manager->input_thread_active = false;
    args->manager->input_thread_complete = true;
    SDL_SignalCondition(args->manager->state_cond);
    SDL_UnlockMutex(args->manager->state_mutex);
}

// Read peer thread - handles SSH data reception with PURE BLOCKING I/O
// Read peer thread - handles SSH server responses
static void read_peer_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;
    char buffer[1024];

    // Signal thread is active
    SDL_LockMutex(args->manager->state_mutex);
    args->manager->peer_thread_active = true;
    SDL_UnlockMutex(args->manager->state_mutex);

    while (true) {
        // Check quit flag with mutex protection
        SDL_LockMutex(args->manager->state_mutex);
        bool should_quit = args->quit;
        bool manager_running = args->manager->thread_running;
        SDL_UnlockMutex(args->manager->state_mutex);

        if (should_quit || !manager_running) {
            break;
        }

        // ssh_client_receive already has internal locking - no external lock needed
        int received = ssh_client_receive(args->ssh_client, buffer, sizeof(buffer) - 1);

        if (received > 0) {
            // Create data event for manager
            ssh_event_t event = {.type = SSH_EVENT_DATA_RECEIVED};
            memcpy(event.data_received.data, buffer, received);
            event.data_received.len = received;
            queue_put_event(args->manager, &event);

        } else if (received == -2) {
            // Clean disconnect
            printf("SSH Peer Thread: Connection closed by remote\n");

            // Set quit flag with mutex protection
            SDL_LockMutex(args->manager->state_mutex);
            args->quit = true;
            SDL_UnlockMutex(args->manager->state_mutex);

            ssh_event_t event = {.type = SSH_EVENT_DISCONNECTED};
            queue_put_event(args->manager, &event);
            break;
        } else if (received < 0) {
            // Error
            printf("SSH Peer Thread: Receive error: %s\n",
                   ssh_client_get_error(args->ssh_client));

            // Set quit flag with mutex protection
            SDL_LockMutex(args->manager->state_mutex);
            args->quit = true;
            SDL_UnlockMutex(args->manager->state_mutex);

            ssh_event_t event = {.type = SSH_EVENT_ERROR};
            strncpy(event.error.message, ssh_client_get_error(args->ssh_client),
                    sizeof(event.error.message) - 1);
            queue_put_event(args->manager, &event);
            break;
        } else {
            // received == 0, no data available - should not happen in pure blocking mode
            // If it does happen, add a small delay to prevent busy waiting
            usleep(10000); // 10ms
        }
    }

    // Signal thread completion
    SDL_LockMutex(args->manager->state_mutex);
    args->manager->peer_thread_active = false;
    args->manager->peer_thread_complete = true;
    SDL_SignalCondition(args->manager->state_cond);
    SDL_UnlockMutex(args->manager->state_mutex);
}

// Wait until both I/O threads have set their _complete flags, or timeout_ms elapses.
// Must NOT be called while already holding state_mutex.
static bool wait_for_io_threads(ssh_thread_manager_t* manager, Sint32 timeout_ms) {
    SDL_LockMutex(manager->state_mutex);
    Uint64 deadline = SDL_GetTicks() + (Uint64)timeout_ms;
    while (!(manager->input_thread_complete && manager->peer_thread_complete)) {
        Uint64 now = SDL_GetTicks();
        if (now >= deadline) break;
        SDL_WaitConditionTimeout(manager->state_cond, manager->state_mutex, (Sint32)(deadline - now));
    }
    bool complete = manager->input_thread_complete && manager->peer_thread_complete;
    SDL_UnlockMutex(manager->state_mutex);
    return complete;
}

// Modified SSH thread main function to set up the two-thread pattern
static void ssh_thread_main(void* data) {
    ssh_thread_manager_t* manager = (ssh_thread_manager_t*)data;
    ssh_cmd_t cmd;
    ssh_event_t event;
    bool ssh_connected = false;

    printf("SSH Thread: Main thread started\n");

    // Initialize SSH client in this thread
    if (!ssh_client_init(&manager->ssh_client)) {
        printf("SSH Main Thread: Failed to initialize SSH client\n");
        event.type = SSH_EVENT_ERROR;
        snprintf(event.error.message, sizeof(event.error.message),
                "Failed to initialize SSH client");
        queue_put_event(manager, &event);

        // Signal completion before exit (error case)
        SDL_LockMutex(manager->state_mutex);
        manager->shutdown_complete = true;
        SDL_SignalCondition(manager->state_cond);
        SDL_UnlockMutex(manager->state_mutex);
        return;
    }

    // Set up auth event callback after SSH client initialization
    ssh_client_set_auth_event_callback(&manager->ssh_client, ssh_auth_event_callback);

    // Signal that worker thread is ready
    SDL_LockMutex(manager->state_mutex);
    manager->worker_ready = true;
    SDL_SignalCondition(manager->state_cond);
    SDL_UnlockMutex(manager->state_mutex);
    printf("SSH Thread: Worker thread ready, starting main loop\n");

    while (true) {
        // Check shutdown status with mutex protection
        SDL_LockMutex(manager->state_mutex);
        bool should_shutdown = manager->shutdown_requested;
        SDL_UnlockMutex(manager->state_mutex);

        if (should_shutdown) {
            break;
        }

        // Process connection commands only (data I/O handled by separate threads)
        if (queue_get_cmd(manager, &cmd)) {
            switch (cmd.type) {
                case SSH_CMD_SEND_RAW_INPUT:
                    // Handled by read_input_thread; main thread should not see these
                    // but re-enqueue if it does to avoid dropping data.
                    if (!queue_put_cmd(manager, &cmd)) {
                        printf("SSH Main Thread: Could not requeue raw input command\n");
                    }
                    break;

                case SSH_CMD_CONNECT: {
                    // ssh_client_cleanup() clears is_initialized; re-init for each connection.
                    if (!manager->ssh_client.is_initialized) {
                        if (!ssh_client_init(&manager->ssh_client)) {
                            printf("SSH Main Thread: Failed to re-initialize SSH client\n");
                            event.type = SSH_EVENT_CONNECTION_FAILED;
                            snprintf(event.error.message, sizeof(event.error.message),
                                    "Failed to re-initialize SSH client");
                            queue_put_event(manager, &event);
                            break;
                        }
                        ssh_client_set_auth_event_callback(&manager->ssh_client, ssh_auth_event_callback);
                    }
                    if (ssh_client_connect_start(&manager->ssh_client,
                                               cmd.connect.hostname, cmd.connect.port,
                                               cmd.connect.username, cmd.connect.password)) {
                        ssh_connected = true;
                        printf("SSH Thread: Connection successful, starting I/O threads\n");

                        manager->thread_args.ssh_client = &manager->ssh_client;
                        manager->thread_args.manager = manager;
                        manager->thread_args.quit = false;

                        SDL_LockMutex(manager->state_mutex);
                        manager->input_thread_active = false;
                        manager->peer_thread_active = false;
                        manager->input_thread_complete = false;
                        manager->peer_thread_complete = false;
                        SDL_UnlockMutex(manager->state_mutex);

                        manager->read_input_thread_id = thread_create(read_input_thread, &manager->thread_args, SSH_IO_THREAD_STACK);
                        manager->read_peer_thread_id = thread_create(read_peer_thread, &manager->thread_args, SSH_IO_THREAD_STACK);

                        if (manager->read_input_thread_id > 0 && manager->read_peer_thread_id > 0) {
                            event.type = SSH_EVENT_CONNECTED;
                            strncpy(event.connected.hostname, cmd.connect.hostname,
                                   sizeof(event.connected.hostname) - 1);
                            strncpy(event.connected.username, cmd.connect.username,
                                   sizeof(event.connected.username) - 1);
                        } else {
                            printf("SSH Main Thread: Failed to start I/O threads\n");

                            // A thread that was never created will never set its _complete flag.
                            // Mark it done preemptively so wait_for_io_threads doesn't block on it.
                            SDL_LockMutex(manager->state_mutex);
                            manager->thread_args.quit = true;
                            if (manager->read_input_thread_id <= 0)
                                manager->input_thread_complete = true;
                            if (manager->read_peer_thread_id <= 0)
                                manager->peer_thread_complete = true;
                            SDL_UnlockMutex(manager->state_mutex);

                            wait_for_io_threads(manager, 2000);
                            ssh_client_cleanup(&manager->ssh_client);
                            ssh_connected = false;
                            manager->read_input_thread_id = 0;
                            manager->read_peer_thread_id = 0;

                            event.type = SSH_EVENT_ERROR;
                            snprintf(event.error.message, sizeof(event.error.message),
                                    "Failed to start I/O threads");
                        }
                    } else {
                        event.type = SSH_EVENT_CONNECTION_FAILED;
                        strncpy(event.error.message, ssh_client_get_error(&manager->ssh_client),
                               sizeof(event.error.message) - 1);
                        printf("SSH Main Thread: Connection failed: %s\n", event.error.message);
                    }
                    queue_put_event(manager, &event);
                    break;
                }

                case SSH_CMD_DISCONNECT: {
                    if (ssh_connected) {
                        printf("SSH Thread: Disconnecting\n");

                        // Signal threads to quit with mutex protection
                        SDL_LockMutex(manager->state_mutex);
                        manager->thread_args.quit = true;
                        SDL_UnlockMutex(manager->state_mutex);

                        // Wait for I/O threads to complete using cooperative termination
                        printf("SSH Thread: Waiting for I/O threads to complete...\n");
                        bool threads_complete = wait_for_io_threads(manager, 2000);
                        if (!threads_complete) {
                            printf("SSH Thread: Warning - I/O threads did not complete within timeout\n");
                        } else {
                            printf("SSH Thread: I/O threads completed successfully\n");
                        }

                        ssh_client_cleanup(&manager->ssh_client);
                        ssh_connected = false;

                        // Reset thread handles
                        manager->read_input_thread_id = 0;
                        manager->read_peer_thread_id = 0;

                        // Do NOT post SSH_EVENT_DISCONNECTED here: the peer thread already
                        // posted it when it detected the remote close, which is what
                        // triggered this SSH_CMD_DISCONNECT in the first place.
                    }
                    break;
                }

                case SSH_CMD_SHUTDOWN: {
                    printf("SSH Thread: Shutdown requested\n");

                    SDL_LockMutex(manager->state_mutex);
                    manager->shutdown_requested = true;
                    SDL_UnlockMutex(manager->state_mutex);

                    if (ssh_connected) {
                        // Signal I/O threads to quit
                        SDL_LockMutex(manager->state_mutex);
                        manager->thread_args.quit = true;
                        SDL_UnlockMutex(manager->state_mutex);

                        if (!wait_for_io_threads(manager, 2000)) {
                            printf("SSH Thread: Warning - I/O threads did not complete during shutdown\n");
                        }
                    }
                    break;
                }
            }
        }

        // Small delay to prevent busy loop
        usleep(SSH_THREAD_POLL_INTERVAL); // 50ms
    }

    printf("SSH Thread: Main thread exiting\n");

    // Cleanup
    if (ssh_connected) {
        // Signal I/O threads to quit
        SDL_LockMutex(manager->state_mutex);
        manager->thread_args.quit = true;
        SDL_UnlockMutex(manager->state_mutex);

        if (!wait_for_io_threads(manager, 2000)) {
            printf("SSH Thread: Warning - I/O threads did not complete during main thread cleanup\n");
        }

        ssh_client_cleanup(&manager->ssh_client);
    }

    // Signal completion before exit
    SDL_LockMutex(manager->state_mutex);
    manager->shutdown_complete = true;
    SDL_SignalCondition(manager->state_cond);
    SDL_UnlockMutex(manager->state_mutex);
    printf("SSH Thread: Worker thread signaled completion\n");
}

// Queue operations (same as original)
static bool queue_put_cmd(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd) {
    if (!manager || !cmd) {
        printf("SSH Thread: queue_put_cmd called with NULL pointers\n");
        return false;
    }

    if (!manager->cmd_queue_mutex) {
        printf("SSH Thread: queue_put_cmd called but mutex not initialized\n");
        return false;
    }

    SDL_LockMutex(manager->cmd_queue_mutex);

    if (manager->cmd_queue_count >= SSH_QUEUE_SIZE) {
        printf("SSH Thread: Command queue full!\n");
        SDL_UnlockMutex(manager->cmd_queue_mutex);
        return false; // Queue full
    }

    manager->cmd_queue[manager->cmd_queue_tail] = *cmd;
    manager->cmd_queue_tail = (manager->cmd_queue_tail + 1) % SSH_QUEUE_SIZE;
    manager->cmd_queue_count++;

    SDL_UnlockMutex(manager->cmd_queue_mutex);
    return true;
}

static bool queue_peek_cmd_type(ssh_thread_manager_t* manager, ssh_cmd_type_t* type) {
    if (!manager || !type || !manager->cmd_queue_mutex) {
        return false;
    }
    SDL_LockMutex(manager->cmd_queue_mutex);
    if (manager->cmd_queue_count == 0) {
        SDL_UnlockMutex(manager->cmd_queue_mutex);
        return false;
    }
    *type = manager->cmd_queue[manager->cmd_queue_head].type;
    SDL_UnlockMutex(manager->cmd_queue_mutex);
    return true;
}

static bool queue_get_cmd(ssh_thread_manager_t* manager, ssh_cmd_t* cmd) {
    if (!manager || !cmd) {
        printf("SSH Thread: queue_get_cmd called with NULL pointers\n");
        return false;
    }

    if (!manager->cmd_queue_mutex) {
        printf("SSH Thread: queue_get_cmd called but mutex not initialized\n");
        return false;
    }

    SDL_LockMutex(manager->cmd_queue_mutex);

    if (manager->cmd_queue_count == 0) {
        SDL_UnlockMutex(manager->cmd_queue_mutex);
        return false; // Queue empty
    }

    *cmd = manager->cmd_queue[manager->cmd_queue_head];
    manager->cmd_queue_head = (manager->cmd_queue_head + 1) % SSH_QUEUE_SIZE;
    manager->cmd_queue_count--;

    SDL_UnlockMutex(manager->cmd_queue_mutex);
    return true;
}

static bool queue_put_event(ssh_thread_manager_t* manager, const ssh_event_t* event) {
    if (!manager || !event) {
        printf("SSH Thread: queue_put_event called with NULL pointers\n");
        return false;
    }

    if (!manager->event_queue_mutex) {
        printf("SSH Thread: queue_put_event called but mutex not initialized\n");
        return false;
    }

    SDL_LockMutex(manager->event_queue_mutex);

    if (manager->event_queue_count >= SSH_QUEUE_SIZE) {
        printf("SSH Thread: Event queue full!\n");
        SDL_UnlockMutex(manager->event_queue_mutex);
        return false; // Queue full
    }

    manager->event_queue[manager->event_queue_tail] = *event;
    manager->event_queue_tail = (manager->event_queue_tail + 1) % SSH_QUEUE_SIZE;
    manager->event_queue_count++;

    SDL_UnlockMutex(manager->event_queue_mutex);
    return true;
}

static bool queue_get_event(ssh_thread_manager_t* manager, ssh_event_t* event) {
    if (!manager || !event) {
        printf("SSH Thread: queue_get_event called with NULL pointers\n");
        return false;
    }

    if (!manager->event_queue_mutex) {
        printf("SSH Thread: queue_get_event called but mutex not initialized\n");
        return false;
    }

    SDL_LockMutex(manager->event_queue_mutex);

    if (manager->event_queue_count == 0) {
        SDL_UnlockMutex(manager->event_queue_mutex);
        return false; // Queue empty
    }

    *event = manager->event_queue[manager->event_queue_head];
    manager->event_queue_head = (manager->event_queue_head + 1) % SSH_QUEUE_SIZE;
    manager->event_queue_count--;

    SDL_UnlockMutex(manager->event_queue_mutex);
    return true;
}

// Auth event callback - called from SSH auth callbacks to generate events immediately
static void ssh_auth_event_callback(ssh_client_t* client, const char* prompt_text, bool echo_input, const char* method_name) {
    (void)client; // Unused parameter - client instance already known via global manager
    ssh_thread_manager_t* manager = NULL;

    // Get manager with mutex protection
    if (g_auth_manager_mutex) {
        SDL_LockMutex(g_auth_manager_mutex);
        manager = g_current_auth_manager;
        SDL_UnlockMutex(g_auth_manager_mutex);
    }

    if (!manager) {
        printf("SSH Thread: Auth event callback called but no manager set\n");
        return;
    }

    ssh_event_t auth_event;
    auth_event.type = SSH_EVENT_AUTH_PROMPT;

    strncpy(auth_event.auth_prompt.prompt_text, prompt_text,
           sizeof(auth_event.auth_prompt.prompt_text) - 1);
    auth_event.auth_prompt.prompt_text[sizeof(auth_event.auth_prompt.prompt_text) - 1] = '\0';

    auth_event.auth_prompt.echo_input = echo_input;

    strncpy(auth_event.auth_prompt.method_name, method_name,
           sizeof(auth_event.auth_prompt.method_name) - 1);
    auth_event.auth_prompt.method_name[sizeof(auth_event.auth_prompt.method_name) - 1] = '\0';

    queue_put_event(manager, &auth_event);
}

// Public API implementation (same as original)
bool ssh_thread_init(ssh_thread_manager_t* manager) {
    if (!manager) {
        printf("SSH Thread Init: manager is NULL\n");
        return false;
    }

    memset(manager, 0, sizeof(ssh_thread_manager_t));

    printf("SSH Thread: Initializing thread manager\n");

    // Initialize coordination flags
    manager->shutdown_complete = false;
    manager->worker_ready = false;

    // Initialize thread state tracking
    manager->read_input_thread_id = 0;
    manager->read_peer_thread_id = 0;
    manager->input_thread_active = false;
    manager->peer_thread_active = false;
    manager->input_thread_complete = false;
    manager->peer_thread_complete = false;

    // Initialize thread arguments structure
    memset(&manager->thread_args, 0, sizeof(manager->thread_args));

    // Create auth manager mutex if not already created (global singleton)
    if (!g_auth_manager_mutex) {
        g_auth_manager_mutex = SDL_CreateMutex();
        if (!g_auth_manager_mutex) {
            printf("SSH Thread Init: Failed to create auth manager mutex: %s\n", SDL_GetError());
            // Not fatal, continue without protected auth
        }
    }

    // Initialize mutexes
    manager->cmd_queue_mutex = SDL_CreateMutex();
    if (!manager->cmd_queue_mutex) {
        printf("SSH Thread Init: Failed to create command queue mutex: %s\n", SDL_GetError());
        return false;
    }

    manager->event_queue_mutex = SDL_CreateMutex();
    if (!manager->event_queue_mutex) {
        printf("SSH Thread Init: Failed to create event queue mutex: %s\n", SDL_GetError());
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        manager->cmd_queue_mutex = NULL;
        return false;
    }

    manager->state_mutex = SDL_CreateMutex();
    if (!manager->state_mutex) {
        printf("SSH Thread Init: Failed to create state mutex: %s\n", SDL_GetError());
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        return false;
    }

    manager->state_cond = SDL_CreateCondition();
    if (!manager->state_cond) {
        printf("SSH Thread Init: Failed to create state condition: %s\n", SDL_GetError());
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        manager->state_mutex = NULL;
        return false;
    }

    // Set auth manager reference with mutex protection
    if (g_auth_manager_mutex) {
        SDL_LockMutex(g_auth_manager_mutex);
        g_current_auth_manager = manager;
        SDL_UnlockMutex(g_auth_manager_mutex);
    }

    manager->ssh_thread_id = thread_create(ssh_thread_main, manager, SSH_MAIN_THREAD_STACK);

    if (manager->ssh_thread_id <= 0) {
        printf("SSH Thread Init: Failed to create thread (invalid pid)\n");
        // Cleanup mutexes on thread creation failure
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        manager->state_mutex = NULL;
        return false;
    }

    manager->thread_running = true;

    // Wait for worker thread to signal ready (with timeout)
    printf("SSH Thread: Waiting for worker thread to become ready...\n");
    SDL_LockMutex(manager->state_mutex);
    Uint64 deadline = SDL_GetTicks() + 2000;
    while (!manager->worker_ready) {
        Uint64 now = SDL_GetTicks();
        if (now >= deadline) break;
        SDL_WaitConditionTimeout(manager->state_cond, manager->state_mutex, (Sint32)(deadline - now));
    }
    bool worker_ready = manager->worker_ready;
    SDL_UnlockMutex(manager->state_mutex);

    if (!worker_ready) {
        printf("SSH Thread Init: Warning - worker thread did not signal ready within timeout\n");
        // Continue anyway - this is non-fatal but worth logging
    } else {
        printf("SSH Thread: Worker thread ready\n");
    }

    printf("SSH Thread: Manager initialized successfully\n");

    return true;
}

void ssh_thread_shutdown(ssh_thread_manager_t* manager) {
    if (!manager || !manager->thread_running) {
        return;
    }

    printf("SSH Thread: Shutting down manager\n");

    // Signal shutdown request using state mutex
    SDL_LockMutex(manager->state_mutex);
    manager->shutdown_requested = true;
    SDL_UnlockMutex(manager->state_mutex);

    // Send shutdown command
    ssh_cmd_t cmd = { .type = SSH_CMD_SHUTDOWN };
    ssh_thread_send_command(manager, &cmd);

    // Wait for worker thread to signal completion (with timeout)
    printf("SSH Thread: Waiting for worker thread to complete...\n");
    SDL_LockMutex(manager->state_mutex);
    Uint64 deadline = SDL_GetTicks() + 5000;
    while (!manager->shutdown_complete) {
        Uint64 now = SDL_GetTicks();
        if (now >= deadline) break;
        SDL_WaitConditionTimeout(manager->state_cond, manager->state_mutex, (Sint32)(deadline - now));
    }
    bool shutdown_complete = manager->shutdown_complete;
    SDL_UnlockMutex(manager->state_mutex);

    if (!shutdown_complete) {
        printf("SSH Thread: Warning - worker thread did not signal completion within timeout\n");
        // Continue with cleanup anyway
    } else {
        printf("SSH Thread: Worker thread completed successfully\n");
    }

    manager->thread_running = false;
    manager->ssh_thread_id = 0;

    // Clear auth manager reference if it matches this manager
    if (g_auth_manager_mutex) {
        SDL_LockMutex(g_auth_manager_mutex);
        if (g_current_auth_manager == manager) {
            g_current_auth_manager = NULL;
        }
        SDL_UnlockMutex(g_auth_manager_mutex);
    }

    // Reset coordination flags
    SDL_LockMutex(manager->state_mutex);
    manager->shutdown_requested = false;
    manager->shutdown_complete = false;
    manager->worker_ready = false;
    SDL_UnlockMutex(manager->state_mutex);

    // Cleanup mutexes and condition variable
    if (manager->cmd_queue_mutex) {
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        manager->cmd_queue_mutex = NULL;
    }
    if (manager->event_queue_mutex) {
        SDL_DestroyMutex(manager->event_queue_mutex);
        manager->event_queue_mutex = NULL;
    }
    if (manager->state_cond) {
        SDL_DestroyCondition(manager->state_cond);
        manager->state_cond = NULL;
    }
    if (manager->state_mutex) {
        SDL_DestroyMutex(manager->state_mutex);
        manager->state_mutex = NULL;
    }

    printf("SSH Thread: Manager shutdown complete\n");
}

bool ssh_thread_send_command(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd) {
    if (!manager || !cmd) {
        printf("SSH Thread: send_command called with NULL pointers\n");
        return false;
    }

    if (!manager->thread_running) {
        printf("SSH Thread: send_command called but thread not running\n");
        return false;
    }

    return queue_put_cmd(manager, cmd);
}

bool ssh_thread_poll_event(ssh_thread_manager_t* manager, ssh_event_t* event) {
    if (!manager || !event || !manager->thread_running) {
        return false;
    }

    return queue_get_event(manager, event);
}

bool ssh_thread_connect(ssh_thread_manager_t* manager, const char* hostname, int port,
                       const char* username, const char* password) {
    if (!manager || !hostname || !username || !password) {
        return false;
    }

    ssh_cmd_t cmd = { .type = SSH_CMD_CONNECT };
    strncpy(cmd.connect.hostname, hostname, sizeof(cmd.connect.hostname) - 1);
    strncpy(cmd.connect.username, username, sizeof(cmd.connect.username) - 1);
    strncpy(cmd.connect.password, password, sizeof(cmd.connect.password) - 1);
    cmd.connect.port = port;

    return ssh_thread_send_command(manager, &cmd);
}

bool ssh_thread_send_raw_input(ssh_thread_manager_t* manager, const char* input, size_t len) {
    if (!manager || !input || len == 0 || len >= sizeof(((ssh_cmd_t*)0)->send_raw_input.input_data)) {
        return false;
    }

    ssh_cmd_t cmd = { .type = SSH_CMD_SEND_RAW_INPUT };
    memcpy(cmd.send_raw_input.input_data, input, len);
    cmd.send_raw_input.input_len = len;

    return ssh_thread_send_command(manager, &cmd);
}

void ssh_thread_disconnect(ssh_thread_manager_t* manager) {
    if (!manager) {
        return;
    }

    ssh_cmd_t cmd = { .type = SSH_CMD_DISCONNECT };
    ssh_thread_send_command(manager, &cmd);
}

bool ssh_thread_is_running(ssh_thread_manager_t* manager) {
    return manager && manager->thread_running;
}

void ssh_thread_submit_auth_response(ssh_thread_manager_t* manager, const char* response) {
    if (!manager || !response) {
        return;
    }
    ssh_client_submit_auth_response(&manager->ssh_client, response);
}
