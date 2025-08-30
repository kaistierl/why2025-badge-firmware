#include "ssh_manager.h"
#include "ssh_thread.h"
#include "../term/term.h"
#include "../ui_manager/ui_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Global SSH thread manager
static ssh_thread_manager_t ssh_thread_mgr;

// Forward declarations
static void ssh_manager_handle_disconnect(app_state_t* app, const char* message, bool show_in_terminal);

// SSH Manager implementation

bool ssh_manager_init(void) {
    printf("SSH Manager: Initializing\n");
    
    // Initialize SSH thread manager
    if (!ssh_thread_init(&ssh_thread_mgr)) {
        printf("SSH Manager: Failed to initialize SSH thread\n");
        return false;
    }
    
    printf("SSH Manager: Initialization complete\n");
    return true;
}

void ssh_manager_shutdown(void) {
    ssh_thread_shutdown(&ssh_thread_mgr);
}

bool ssh_manager_connect(app_state_t* app, const char* hostname, int port, 
                        const char* username, const char* password) {
    if (!app || !hostname || !username || !password) {
        return false;
    }
    
    ui_manager_show_connecting_message();
    
    // Validate and set default port if needed
    int safe_port = (port > 0 && port <= 65535) ? port : 22;
    
    // Send connect command to SSH thread (non-blocking)
    if (!ssh_thread_connect(&ssh_thread_mgr, hostname, safe_port, username, password)) {
        ui_manager_show_connection_error("Failed to send connection request to SSH thread");
        return false;
    }
    
    // Mark as connecting - ssh_manager_poll_and_read() will handle progress
    app->ssh_connecting = true;
    return true;
}

void ssh_manager_disconnect(app_state_t* app) {
    if (!app) {
        return;
    }
    
    if (app->ssh_connected || app->ssh_connecting) {
        // Send disconnect command to SSH thread
        ssh_thread_disconnect(&ssh_thread_mgr);
    }
    
    app->ssh_connected = false;
    app->ssh_connecting = false;
}

bool ssh_manager_is_connected(app_state_t* app) {
    return app && app->ssh_connected;
}

bool ssh_manager_send_data(app_state_t* app, const char* data, size_t len) {
    if (!ssh_manager_is_connected(app)) {
        return false;
    }
    
    // Send data to SSH thread (non-blocking)
    if (!ssh_thread_send_data(&ssh_thread_mgr, data, len)) {
        return false;
    }
    
    return true;
}

bool ssh_manager_poll_and_read(app_state_t* app) {
    if (!app) {
        return false;
    }
    
    // Poll for events from SSH thread (non-blocking)
    ssh_event_t event;
    bool data_received = false;
    
    while (ssh_thread_poll_event(&ssh_thread_mgr, &event)) {
        switch (event.type) {
            case SSH_EVENT_CONNECTED: {
                printf("SSH Manager: Connection established\n");
                app->ssh_connected = true;
                app->ssh_connecting = false;
                app->had_ssh_session = true;
                
                ui_manager_show_connection_success(event.connected.hostname, event.connected.username);
                data_received = true;
                break;
            }
            
            case SSH_EVENT_CONNECTION_FAILED: {
                printf("SSH Manager: Connection failed: %s\n", event.error.message);
                ui_manager_show_connection_error(event.error.message);
                app->ssh_connecting = false;
                break;
            }
            
            case SSH_EVENT_DATA_RECEIVED: {
                // Forward data to terminal
                term_input_bytes((const uint8_t*)event.data_received.data, event.data_received.len);
                data_received = true;
                break;
            }
            
            case SSH_EVENT_DISCONNECTED: {
                printf("SSH Manager: Connection closed by remote\n");
                ssh_manager_handle_disconnect(app, "Connection closed by remote host", true);
                break;
            }
            
            case SSH_EVENT_ERROR: {
                ssh_manager_handle_disconnect(app, event.error.message, true);
                break;
            }
        }
    }
    
    return data_received;
}

// Helper function to handle disconnection with consistent cleanup
static void ssh_manager_handle_disconnect(app_state_t* app, const char* message, bool show_in_terminal) {
    if (!app || !message) {
        return;
    }
    
    // Output to console
    printf("%s\n", message);
    
    // Show in terminal if requested
    if (show_in_terminal) {
        term_input_string("\r\n");
        term_input_string(message);
        if (message[strlen(message) - 1] != '\n') {
            term_input_string("\r\n");
        }
    }
    
    // Cleanup
    ssh_manager_cleanup(app);
}

bool ssh_manager_is_connecting(app_state_t* app) {
    return app && app->ssh_connecting;
}

const char* ssh_manager_get_error(app_state_t* app) {
    if (!app) {
        return "Invalid app state";
    }
    // In threaded mode, specific error messages come through events
    return "SSH thread error - check event messages";
}

void ssh_manager_cleanup(app_state_t* app) {
    if (!app) {
        return;
    }
    
    if (app->ssh_connected || app->ssh_connecting) {
        // Send disconnect command to SSH thread
        ssh_thread_disconnect(&ssh_thread_mgr);
    }
    
    app->ssh_connected = false;
    app->ssh_connecting = false;
}
