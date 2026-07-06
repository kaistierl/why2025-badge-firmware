/* ssh_thread.c - Threaded SSH I/O manager implementation
 *
 * Three-thread model (Option A — cipher ring buffer):
 *
 *   ssh_thread_main      — control commands only (CONNECT/DISCONNECT/SHUTDOWN)
 *   sock_reader_thread   — sole caller of read(socket_fd); writes raw cipher bytes
 *                          into ssh_client_t::cipher_rb. No wolfSSH calls.
 *   ssh_io_thread        — sole wolfSSH caller. Drains cipher_rb via
 *                          wolfSSH_stream_read (non-blocking, WS_CBIO_ERR_WANT_READ
 *                          when empty) and writes to term_buf. Also processes
 *                          SSH_CMD_SEND_RAW_INPUT via wolfSSH_stream_send.
 *
 * Eliminating concurrent wolfSSH access removes the send+receive race that caused
 * crashes under sustained input against high-throughput servers (htop).
 */

#include "ssh_thread.h"
#include "badgevms/process.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>

// Forward declarations
static void sock_reader_thread(void* arg);
static void ssh_io_thread(void* arg);
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

// Write bytes into the terminal ring buffer. Drops data if full (logs once per burst).
// Returns true if all bytes were written, false if some were dropped.
static bool term_buf_write(ssh_thread_manager_t* manager, const uint8_t* data, size_t len) {
    SDL_LockMutex(manager->term_buf_mutex);

    size_t space = SSH_TERMINAL_RING_BUF_SIZE - manager->term_buf_count;
    if (len > space) {
        SDL_UnlockMutex(manager->term_buf_mutex);
        return false; // caller logs the drop
    }

    size_t first = SSH_TERMINAL_RING_BUF_SIZE - manager->term_buf_tail;
    if (len <= first) {
        memcpy(manager->term_buf + manager->term_buf_tail, data, len);
    } else {
        memcpy(manager->term_buf + manager->term_buf_tail, data, first);
        memcpy(manager->term_buf, data + first, len - first);
    }
    manager->term_buf_tail  = (manager->term_buf_tail + len) % SSH_TERMINAL_RING_BUF_SIZE;
    manager->term_buf_count += len;

    SDL_UnlockMutex(manager->term_buf_mutex);
    return true;
}

// sock_reader_thread — the ONLY thread that calls read() on the socket.
// Writes raw cipher bytes into ssh_client_t::cipher_rb and signals ssh_io_thread.
// No wolfSSH calls; no shared SSH state accessed.
static void sock_reader_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;
    uint8_t buf[SSH_DATA_BUFFER_SIZE];

    SDL_LockMutex(args->manager->state_mutex);
    args->manager->sock_reader_active = true;
    SDL_UnlockMutex(args->manager->state_mutex);

    int sock_fd = ssh_client_get_fd(args->ssh_client);

    while (true) {
        SDL_LockMutex(args->manager->state_mutex);
        bool should_quit = args->quit;
        SDL_UnlockMutex(args->manager->state_mutex);
        if (should_quit) break;

        int n = (int)read(sock_fd, buf, sizeof(buf));

        if (n > 0) {
            // Blocking write — applies TCP backpressure rather than dropping bytes.
            // cipher_rb must be lossless: dropping cipher bytes corrupts the wolfSSH stream.
            ssh_client_cipher_rb_write(args->ssh_client, buf, n);
        } else {
            // n == 0: clean TCP close; n < 0: read error (includes disconnect during cleanup).
            if (n < 0 && !args->quit) {
                printf("SSH Sock Reader: read() error: %s (errno=%d)\n", strerror(errno), errno);
            }
            // Mark socket closed so cipher_rb_write unblocks and wolfssh_io_recv_streaming
            // returns WS_CBIO_ERR_CONN_CLOSE on the next drain attempt.
            args->ssh_client->socket_closed = true;
            // Unblock any thread waiting in cipher_rb_write (which checks socket_closed).
            SDL_LockMutex((SDL_Mutex*)args->ssh_client->cipher_rb_mutex);
            SDL_SignalCondition((SDL_Condition*)args->ssh_client->cipher_rb_space_cond);
            SDL_UnlockMutex((SDL_Mutex*)args->ssh_client->cipher_rb_mutex);
        }

        // Wake ssh_io_thread: new cipher data available, or socket closed.
        SDL_LockMutex(args->manager->cmd_queue_mutex);
        SDL_SignalCondition(args->manager->io_work_cond);
        SDL_UnlockMutex(args->manager->cmd_queue_mutex);

        if (n <= 0) break;
    }

    SDL_LockMutex(args->manager->state_mutex);
    args->manager->sock_reader_active = false;
    args->manager->sock_reader_complete = true;
    SDL_SignalCondition(args->manager->state_cond);
    SDL_UnlockMutex(args->manager->state_mutex);
}

// ssh_io_thread — the SOLE wolfSSH caller after the handshake.
// Drains cipher_rb through wolfSSH_stream_read → term_buf, and processes sends.
// No concurrent access to WOLFSSH* is possible; no wolfssh_mutex needed.
static void ssh_io_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;
    char recv_buf[SSH_DATA_BUFFER_SIZE];
    ssh_event_t event;
    memset(&event, 0, sizeof(event));

    SDL_LockMutex(args->manager->state_mutex);
    args->manager->io_thread_active = true;
    SDL_UnlockMutex(args->manager->state_mutex);

    while (true) {
        SDL_LockMutex(args->manager->state_mutex);
        bool should_quit = args->quit;
        SDL_UnlockMutex(args->manager->state_mutex);
        if (should_quit) break;

        bool did_work = false;

        // Drain all available cipher data through wolfSSH (non-blocking).
        // wolfssh_io_recv_streaming returns WS_CBIO_ERR_WANT_READ when cipher_rb is
        // empty, causing wolfSSH_stream_read → WS_WANT_READ → receive returns 0.
        int received;
        do {
            received = ssh_client_receive(args->ssh_client, recv_buf, sizeof(recv_buf) - 1);
            if (received > 0) {
                if (!term_buf_write(args->manager, (const uint8_t*)recv_buf, received)) {
                    printf("SSH IO Thread: terminal ring buffer full, dropping %d bytes\n", received);
                }
                did_work = true;
            }
        } while (received > 0);

        if (received == -2) {
            printf("SSH IO Thread: Connection closed by remote\n");
            SDL_LockMutex(args->manager->state_mutex);
            args->quit = true;
            SDL_UnlockMutex(args->manager->state_mutex);
            event.type = SSH_EVENT_DISCONNECTED;
            queue_put_event(args->manager, &event);
            break;
        } else if (received < 0) {
            printf("SSH IO Thread: Receive error: %s\n", ssh_client_get_error(args->ssh_client));
            SDL_LockMutex(args->manager->state_mutex);
            args->quit = true;
            SDL_UnlockMutex(args->manager->state_mutex);
            event.type = SSH_EVENT_ERROR;
            strncpy(event.error.message, ssh_client_get_error(args->ssh_client),
                    sizeof(event.error.message) - 1);
            queue_put_event(args->manager, &event);
            break;
        }

        // Process all pending sends (sole wolfSSH caller — no mutex needed).
        ssh_cmd_type_t next_type;
        while (queue_peek_cmd_type(args->manager, &next_type) &&
               next_type == SSH_CMD_SEND_RAW_INPUT) {
            ssh_cmd_t cmd;
            if (queue_get_cmd(args->manager, &cmd)) {
                bool sent = ssh_client_send(args->ssh_client,
                                            cmd.send_raw_input.input_data,
                                            cmd.send_raw_input.input_len);
                if (!sent) {
                    printf("SSH IO Thread: Failed to send input: %s\n",
                           ssh_client_get_error(args->ssh_client));
                    ssh_event_t err = {.type = SSH_EVENT_ERROR};
                    strncpy(err.error.message, ssh_client_get_error(args->ssh_client),
                            sizeof(err.error.message) - 1);
                    queue_put_event(args->manager, &err);
                }
                did_work = true;
            }
        }

        // If nothing happened this cycle, wait for cipher data or a send command.
        // io_work_cond is signalled by sock_reader_thread (new bytes) and queue_put_cmd
        // (new SSH_CMD_SEND_RAW_INPUT). Uses cmd_queue_mutex as the associated mutex.
        if (!did_work) {
            SDL_LockMutex(args->manager->cmd_queue_mutex);
            SDL_WaitConditionTimeout(args->manager->io_work_cond,
                                     args->manager->cmd_queue_mutex, 5);
            SDL_UnlockMutex(args->manager->cmd_queue_mutex);
        }
    }

    SDL_LockMutex(args->manager->state_mutex);
    args->manager->io_thread_active = false;
    args->manager->io_thread_complete = true;
    SDL_SignalCondition(args->manager->state_cond);
    SDL_UnlockMutex(args->manager->state_mutex);
}

// Wait until both I/O threads have set their _complete flags, or timeout_ms elapses.
// Must NOT be called while already holding state_mutex.
static bool wait_for_io_threads(ssh_thread_manager_t* manager, Sint32 timeout_ms) {
    SDL_LockMutex(manager->state_mutex);
    Uint64 deadline = SDL_GetTicks() + (Uint64)timeout_ms;
    while (!(manager->io_thread_complete && manager->sock_reader_complete)) {
        Uint64 now = SDL_GetTicks();
        if (now >= deadline) break;
        SDL_WaitConditionTimeout(manager->state_cond, manager->state_mutex, (Sint32)(deadline - now));
    }
    bool complete = manager->io_thread_complete && manager->sock_reader_complete;
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

        // Process control commands only; SSH_CMD_SEND_RAW_INPUT belongs to ssh_io_thread.
        // Peek first so we never dequeue a raw-input command — doing so and re-enqueuing it
        // creates a tight re-enqueue loop on multi-core hardware that starves the input
        // thread and floods the condition variable with signals, corrupting wolfSSH state.
        ssh_cmd_type_t peek_type;
        if (queue_peek_cmd_type(manager, &peek_type) &&
            peek_type != SSH_CMD_SEND_RAW_INPUT &&
            queue_get_cmd(manager, &cmd)) {

            switch (cmd.type) {
                case SSH_CMD_SEND_RAW_INPUT:
                    // Unreachable: filtered out by the peek above.
                    break;

                case SSH_CMD_CONNECT: {
                    // Snapshot the generation at the start of this attempt. All events
                    // posted below are tagged with my_gen so the main thread can discard
                    // events from a connection that was cancelled before it completed.
                    uint32_t my_gen = manager->connect_gen;
                    manager->event_tag_gen = my_gen;

                    // ssh_client_cleanup() clears is_initialized; re-init for each connection.
                    if (!manager->ssh_client.is_initialized) {
                        if (!ssh_client_init(&manager->ssh_client)) {
                            printf("SSH Main Thread: Failed to re-initialize SSH client\n");
                            event.type = SSH_EVENT_CONNECTION_FAILED;
                            event.gen = my_gen;
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
                        manager->io_thread_active    = false;
                        manager->sock_reader_active  = false;
                        manager->io_thread_complete  = false;
                        manager->sock_reader_complete = false;
                        SDL_UnlockMutex(manager->state_mutex);

                        manager->io_thread_id = thread_create(ssh_io_thread, &manager->thread_args, SSH_IO_THREAD_STACK);
                        manager->sock_reader_thread_id = thread_create(sock_reader_thread, &manager->thread_args, SSH_IO_THREAD_STACK);

                        if (manager->io_thread_id > 0 && manager->sock_reader_thread_id > 0) {
                            event.type = SSH_EVENT_CONNECTED;
                            event.gen = my_gen;
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
                            if (manager->io_thread_id <= 0)
                                manager->io_thread_complete = true;
                            if (manager->sock_reader_thread_id <= 0)
                                manager->sock_reader_complete = true;
                            SDL_UnlockMutex(manager->state_mutex);

                            wait_for_io_threads(manager, 2000);
                            ssh_client_cleanup(&manager->ssh_client);
                            ssh_connected = false;
                            manager->io_thread_id = 0;
                            manager->sock_reader_thread_id = 0;

                            event.type = SSH_EVENT_ERROR;
                            event.gen = my_gen;
                            snprintf(event.error.message, sizeof(event.error.message),
                                    "Failed to start I/O threads");
                        }
                    } else {
                        event.type = SSH_EVENT_CONNECTION_FAILED;
                        event.gen = my_gen;
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
                        manager->io_thread_id = 0;
                        manager->sock_reader_thread_id = 0;

                        // Do NOT post SSH_EVENT_DISCONNECTED here: the peer thread already
                        // posted it when it detected the remote close, which is what
                        // triggered this SSH_CMD_DISCONNECT in the first place.
                    } else if (manager->ssh_client.is_initialized) {
                        // The connect attempt finished (or failed) but never fully
                        // established. Clean up the ssh_client so the next SSH_CMD_CONNECT
                        // gets a fresh, known-good client rather than a dirty one.
                        printf("SSH Thread: Cleaning up ssh_client after aborted/failed connect\n");
                        ssh_client_cleanup(&manager->ssh_client);
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

        // Block until a control command arrives or the quit-check timeout fires.
        // Also wait when SSH_CMD_SEND_RAW_INPUT is at the head (ssh_io_thread owns
        // those; waking here only wastes cycles and adds mutex contention).
        SDL_LockMutex(manager->cmd_queue_mutex);
        if (manager->cmd_queue_count == 0 ||
            manager->cmd_queue[manager->cmd_queue_head].type == SSH_CMD_SEND_RAW_INPUT) {
            SDL_WaitConditionTimeout(manager->cmd_cond,
                                     manager->cmd_queue_mutex, 100);
        }
        SDL_UnlockMutex(manager->cmd_queue_mutex);
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

    if (manager->cmd_queue_count >= SSH_CMD_QUEUE_SIZE) {
        printf("SSH Thread: Command queue full!\n");
        SDL_UnlockMutex(manager->cmd_queue_mutex);
        return false; // Queue full
    }

    manager->cmd_queue[manager->cmd_queue_tail] = *cmd;
    manager->cmd_queue_tail = (manager->cmd_queue_tail + 1) % SSH_CMD_QUEUE_SIZE;
    manager->cmd_queue_count++;

    // Route the wakeup to the right consumer: raw-input commands wake ssh_io_thread;
    // control commands wake ssh_thread_main. io_work_cond uses cmd_queue_mutex as its
    // associated mutex, so the signal is safe to emit while the mutex is held here.
    if (cmd->type == SSH_CMD_SEND_RAW_INPUT) {
        SDL_SignalCondition(manager->io_work_cond);
    } else {
        SDL_SignalCondition(manager->cmd_cond);
    }
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
    manager->cmd_queue_head = (manager->cmd_queue_head + 1) % SSH_CMD_QUEUE_SIZE;
    manager->cmd_queue_count--;

    // After removing an item the next item at the head may be a control command
    // that the main SSH thread is waiting for. Signal cmd_cond so it can check.
    SDL_SignalCondition(manager->cmd_cond);
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

    if (manager->event_queue_count >= SSH_EVENT_QUEUE_SIZE) {
        SDL_UnlockMutex(manager->event_queue_mutex);
        return false; // Queue full — caller retries with backpressure
    }

    manager->event_queue[manager->event_queue_tail] = *event;
    manager->event_queue_tail = (manager->event_queue_tail + 1) % SSH_EVENT_QUEUE_SIZE;
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
    manager->event_queue_head = (manager->event_queue_head + 1) % SSH_EVENT_QUEUE_SIZE;
    manager->event_queue_count--;

    SDL_SignalCondition(manager->event_queue_not_full_cond);
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
    auth_event.gen = manager->event_tag_gen;

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
    manager->io_thread_id = 0;
    manager->sock_reader_thread_id = 0;
    manager->io_thread_active    = false;
    manager->sock_reader_active  = false;
    manager->io_thread_complete  = false;
    manager->sock_reader_complete = false;

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

    manager->cmd_cond = SDL_CreateCondition();
    if (!manager->cmd_cond) {
        printf("SSH Thread Init: Failed to create command condition: %s\n", SDL_GetError());
        SDL_DestroyCondition(manager->state_cond);
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->state_cond = NULL;
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        manager->state_mutex = NULL;
        return false;
    }

    manager->io_work_cond = SDL_CreateCondition();
    if (!manager->io_work_cond) {
        printf("SSH Thread Init: Failed to create io_work condition: %s\n", SDL_GetError());
        SDL_DestroyCondition(manager->cmd_cond);
        SDL_DestroyCondition(manager->state_cond);
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->cmd_cond = NULL;
        manager->state_cond = NULL;
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        manager->state_mutex = NULL;
        return false;
    }

    manager->event_queue_not_full_cond = SDL_CreateCondition();
    if (!manager->event_queue_not_full_cond) {
        printf("SSH Thread Init: Failed to create event-queue-not-full condition: %s\n", SDL_GetError());
        SDL_DestroyCondition(manager->io_work_cond);
        SDL_DestroyCondition(manager->cmd_cond);
        SDL_DestroyCondition(manager->state_cond);
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->io_work_cond = NULL;
        manager->cmd_cond = NULL;
        manager->state_cond = NULL;
        manager->cmd_queue_mutex = NULL;
        manager->event_queue_mutex = NULL;
        manager->state_mutex = NULL;
        return false;
    }

    manager->term_buf_mutex = SDL_CreateMutex();
    if (!manager->term_buf_mutex) {
        printf("SSH Thread Init: Failed to create terminal ring buffer mutex: %s\n", SDL_GetError());
        SDL_DestroyCondition(manager->event_queue_not_full_cond);
        SDL_DestroyCondition(manager->io_work_cond);
        SDL_DestroyCondition(manager->cmd_cond);
        SDL_DestroyCondition(manager->state_cond);
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->event_queue_not_full_cond = NULL;
        manager->io_work_cond = NULL;
        manager->cmd_cond = NULL;
        manager->state_cond = NULL;
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
        SDL_DestroyMutex(manager->term_buf_mutex);
        SDL_DestroyCondition(manager->event_queue_not_full_cond);
        SDL_DestroyCondition(manager->io_work_cond);
        SDL_DestroyCondition(manager->cmd_cond);
        SDL_DestroyCondition(manager->state_cond);
        SDL_DestroyMutex(manager->cmd_queue_mutex);
        SDL_DestroyMutex(manager->event_queue_mutex);
        SDL_DestroyMutex(manager->state_mutex);
        manager->term_buf_mutex = NULL;
        manager->event_queue_not_full_cond = NULL;
        manager->io_work_cond = NULL;
        manager->cmd_cond = NULL;
        manager->state_cond = NULL;
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

    // Cleanup mutexes and condition variables
    if (manager->term_buf_mutex) {
        SDL_DestroyMutex(manager->term_buf_mutex);
        manager->term_buf_mutex = NULL;
    }
    if (manager->event_queue_not_full_cond) {
        SDL_DestroyCondition(manager->event_queue_not_full_cond);
        manager->event_queue_not_full_cond = NULL;
    }
    if (manager->io_work_cond) {
        SDL_DestroyCondition(manager->io_work_cond);
        manager->io_work_cond = NULL;
    }
    if (manager->cmd_cond) {
        SDL_DestroyCondition(manager->cmd_cond);
        manager->cmd_cond = NULL;
    }
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

size_t ssh_thread_drain_terminal_data(ssh_thread_manager_t* manager, uint8_t* buf, size_t buf_size) {
    if (!manager || !buf || buf_size == 0 || !manager->term_buf_mutex) {
        return 0;
    }

    SDL_LockMutex(manager->term_buf_mutex);

    size_t to_read = manager->term_buf_count < buf_size ? manager->term_buf_count : buf_size;
    if (to_read == 0) {
        SDL_UnlockMutex(manager->term_buf_mutex);
        return 0;
    }

    size_t first = SSH_TERMINAL_RING_BUF_SIZE - manager->term_buf_head;
    if (to_read <= first) {
        memcpy(buf, manager->term_buf + manager->term_buf_head, to_read);
    } else {
        memcpy(buf, manager->term_buf + manager->term_buf_head, first);
        memcpy(buf + first, manager->term_buf, to_read - first);
    }
    manager->term_buf_head  = (manager->term_buf_head + to_read) % SSH_TERMINAL_RING_BUF_SIZE;
    manager->term_buf_count -= to_read;

    SDL_UnlockMutex(manager->term_buf_mutex);
    return to_read;
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
