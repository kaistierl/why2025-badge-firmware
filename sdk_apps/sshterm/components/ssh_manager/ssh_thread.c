/* ssh_thread.c - Threaded SSH I/O manager implementation 
 * Pure blocking I/O version without select() for BadgeVMS compatibility
 */

#include "ssh_thread.h"
#include "badgevms/process.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Forward declarations
static void read_input_thread(void* arg);
static void read_peer_thread(void* arg);
static void ssh_thread_main(void* data);
static bool queue_put_cmd(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd);
static bool queue_get_cmd(ssh_thread_manager_t* manager, ssh_cmd_t* cmd);
static bool queue_put_event(ssh_thread_manager_t* manager, const ssh_event_t* event);
static bool queue_get_event(ssh_thread_manager_t* manager, ssh_event_t* event);

// Thread arguments structure
typedef struct {
    ssh_client_t* ssh_client;
    ssh_thread_manager_t* manager;
    bool quit;
} ssh_thread_args_t;

// Thread handles
static pid_t read_input_thread_id = 0;
static pid_t read_peer_thread_id = 0;
static ssh_thread_args_t thread_args = {0};

// Command queue for input thread
typedef struct {
    char command[256];
    bool has_command;
} ssh_command_t;

static ssh_command_t command_queue[10];
static int queue_head = 0;
static int queue_tail = 0;

static bool queue_empty(void) {
    return queue_head == queue_tail;
}

static bool queue_full(void) {
    return ((queue_tail + 1) % 10) == queue_head;
}

static bool enqueue_command(const char* command) {
    if (queue_full()) {
        return false;
    }
    
    strncpy(command_queue[queue_tail].command, command, sizeof(command_queue[queue_tail].command) - 1);
    command_queue[queue_tail].command[sizeof(command_queue[queue_tail].command) - 1] = '\0';
    command_queue[queue_tail].has_command = true;
    
    queue_tail = (queue_tail + 1) % 10;
    return true;
}

static bool dequeue_command(char* command, size_t size) {
    if (queue_empty()) {
        return false;
    }
    
    strncpy(command, command_queue[queue_head].command, size - 1);
    command[size - 1] = '\0';
    
    queue_head = (queue_head + 1) % 10;
    return true;
}

// Read input thread - handles keyboard input and sends to SSH
static void read_input_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;
    char command[256];
    
    while (!args->quit && args->manager->thread_running) {
        // Check for queued commands (from keyboard input)
        if (dequeue_command(command, sizeof(command))) {
            // Send command to SSH server
            bool sent = ssh_client_send(args->ssh_client, command, strlen(command));
            
            if (!sent) {
                printf("SSH Input Thread: Failed to send command: %s\n", 
                       ssh_client_get_error(args->ssh_client));
                
                // Signal error to manager
                ssh_event_t event = {.type = SSH_EVENT_ERROR};
                strncpy(event.error.message, ssh_client_get_error(args->ssh_client), 
                        sizeof(event.error.message) - 1);
                queue_put_event(args->manager, &event);
            }
        }
        
        // Small delay to prevent busy waiting
        usleep(SSH_THREAD_POLL_INTERVAL); // 50ms
    }
}

// Read peer thread - handles SSH data reception with PURE BLOCKING I/O
// Read peer thread - handles SSH server responses
static void read_peer_thread(void* arg) {
    ssh_thread_args_t* args = (ssh_thread_args_t*)arg;
    char buffer[1024];
    
    while (!args->quit && args->manager->thread_running) {
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
            args->quit = true;
            
            ssh_event_t event = {.type = SSH_EVENT_DISCONNECTED};
            queue_put_event(args->manager, &event);
            break;
        } else if (received < 0) {
            // Error
            printf("SSH Peer Thread: Receive error: %s\n", 
                   ssh_client_get_error(args->ssh_client));
            args->quit = true;
            
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
        return;
    }
    
    while (!manager->shutdown_requested) {
        // Process connection commands only (data I/O handled by separate threads)
        if (queue_get_cmd(manager, &cmd)) {
            switch (cmd.type) {
                case SSH_CMD_CONNECT: {
                    if (ssh_client_connect_start(&manager->ssh_client, 
                                               cmd.connect.hostname, cmd.connect.port,
                                               cmd.connect.username, cmd.connect.password)) {
                        
                        if (!ssh_client_connect_continue(&manager->ssh_client)) {
                            if (ssh_client_get_state(&manager->ssh_client) == SSH_STATE_CONNECTED) {
                                ssh_connected = true;
                                printf("SSH Thread: Connection successful, starting I/O threads\n");
                                
                                // Start the two I/O threads
                                thread_args.ssh_client = &manager->ssh_client;
                                thread_args.manager = manager;
                                thread_args.quit = false;
                                
                                read_input_thread_id = thread_create(read_input_thread, &thread_args, SSH_IO_THREAD_STACK);
                                read_peer_thread_id = thread_create(read_peer_thread, &thread_args, SSH_IO_THREAD_STACK);
                                
                                if (read_input_thread_id > 0 && read_peer_thread_id > 0) {
                                    event.type = SSH_EVENT_CONNECTED;
                                    strncpy(event.connected.hostname, cmd.connect.hostname, 
                                           sizeof(event.connected.hostname) - 1);
                                    strncpy(event.connected.username, cmd.connect.username, 
                                           sizeof(event.connected.username) - 1);
                                } else {
                                    printf("SSH Main Thread: Failed to start I/O threads\n");
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
                        }
                    } else {
                        event.type = SSH_EVENT_CONNECTION_FAILED;
                        strncpy(event.error.message, ssh_client_get_error(&manager->ssh_client), 
                               sizeof(event.error.message) - 1);
                        printf("SSH Main Thread: Connection start failed: %s\n", event.error.message);
                        queue_put_event(manager, &event);
                    }
                    break;
                }
                
                case SSH_CMD_SEND_DATA: {
                    if (ssh_connected) {
                        // Queue the command for the input thread
                        if (!enqueue_command(cmd.send_data.data)) {
                            printf("SSH Main Thread: Input queue full!\n");
                        }
                    }
                    break;
                }
                
                case SSH_CMD_DISCONNECT: {
                    if (ssh_connected) {
                        printf("SSH Thread: Disconnecting\n");
                        
                        // Signal threads to quit
                        thread_args.quit = true;
                        
                        // Wait a bit for threads to exit
                        usleep(SSH_THREAD_SHUTDOWN_WAIT); // 100ms
                        
                        ssh_client_cleanup(&manager->ssh_client);
                        ssh_connected = false;
                        
                        event.type = SSH_EVENT_DISCONNECTED;
                        queue_put_event(manager, &event);
                    }
                    break;
                }
                
                case SSH_CMD_SHUTDOWN: {
                    printf("SSH Thread: Shutdown requested\n");
                    manager->shutdown_requested = true;
                    
                    if (ssh_connected) {
                        thread_args.quit = true;
                        usleep(SSH_THREAD_SHUTDOWN_WAIT); // 100ms
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
        thread_args.quit = true;
        usleep(SSH_THREAD_SHUTDOWN_WAIT); // 100ms
        ssh_client_cleanup(&manager->ssh_client);
    }
}

// Queue operations (same as original)
static bool queue_put_cmd(ssh_thread_manager_t* manager, const ssh_cmd_t* cmd) {
    if (manager->cmd_queue_count >= SSH_QUEUE_SIZE) {
        printf("SSH Thread: Command queue full!\n");
        return false; // Queue full
    }
    
    manager->cmd_queue[manager->cmd_queue_tail] = *cmd;
    manager->cmd_queue_tail = (manager->cmd_queue_tail + 1) % SSH_QUEUE_SIZE;
    manager->cmd_queue_count++;
    
    return true;
}

static bool queue_get_cmd(ssh_thread_manager_t* manager, ssh_cmd_t* cmd) {
    if (manager->cmd_queue_count == 0) {
        return false; // Queue empty
    }
    
    *cmd = manager->cmd_queue[manager->cmd_queue_head];
    manager->cmd_queue_head = (manager->cmd_queue_head + 1) % SSH_QUEUE_SIZE;
    manager->cmd_queue_count--;
    
    return true;
}

static bool queue_put_event(ssh_thread_manager_t* manager, const ssh_event_t* event) {
    if (manager->event_queue_count >= SSH_QUEUE_SIZE) {
        return false; // Queue full
    }
    
    manager->event_queue[manager->event_queue_tail] = *event;
    manager->event_queue_tail = (manager->event_queue_tail + 1) % SSH_QUEUE_SIZE;
    manager->event_queue_count++;
    return true;
}

static bool queue_get_event(ssh_thread_manager_t* manager, ssh_event_t* event) {
    if (manager->event_queue_count == 0) {
        return false; // Queue empty
    }
    
    *event = manager->event_queue[manager->event_queue_head];
    manager->event_queue_head = (manager->event_queue_head + 1) % SSH_QUEUE_SIZE;
    manager->event_queue_count--;
    return true;
}

// Public API implementation (same as original)
bool ssh_thread_init(ssh_thread_manager_t* manager) {
    if (!manager) {
        printf("SSH Thread Init: manager is NULL\n");
        return false;
    }
    
    memset(manager, 0, sizeof(ssh_thread_manager_t));
    
    printf("SSH Thread: Initializing thread manager\n");
    
    manager->ssh_thread_id = thread_create(ssh_thread_main, manager, SSH_MAIN_THREAD_STACK);
    
    if (manager->ssh_thread_id <= 0) {
        printf("SSH Thread Init: Failed to create thread (invalid pid)\n");
        return false;
    }
    
    manager->thread_running = true;
    
    printf("SSH Thread: Manager initialized successfully\n");
    
    // Give the thread a moment to start up
    usleep(SSH_THREAD_STARTUP_DELAY); // 50ms
    
    return true;
}

void ssh_thread_shutdown(ssh_thread_manager_t* manager) {
    if (!manager || !manager->thread_running) {
        return;
    }
    
    printf("SSH Thread: Shutting down manager\n");
    
    // Send shutdown command
    ssh_cmd_t cmd = { .type = SSH_CMD_SHUTDOWN };
    ssh_thread_send_command(manager, &cmd);
    
    // Wait a bit for thread to exit gracefully
    usleep(SSH_THREAD_SHUTDOWN_WAIT); // 100ms
    
    manager->thread_running = false;
    manager->ssh_thread_id = 0;
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

bool ssh_thread_send_data(ssh_thread_manager_t* manager, const char* data, size_t len) {
    if (!manager || !data || len == 0 || len >= SSH_DATA_BUFFER_SIZE) {
        return false;
    }
    
    ssh_cmd_t cmd = { .type = SSH_CMD_SEND_DATA };
    memcpy(cmd.send_data.data, data, len);
    cmd.send_data.len = len;
    
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

bool ssh_thread_is_connected(ssh_thread_manager_t* manager) {
    return manager && manager->thread_running;
}
