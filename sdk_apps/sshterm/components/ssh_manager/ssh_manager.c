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
    ui_manager_reset_auth_header();

    // Validate and set default port if needed
    int safe_port = (port > 0 && port <= 65535) ? port : 22;

    // Advance the generation so events from any previous (aborted) attempt are discarded.
    ssh_thread_mgr.connect_gen++;

    // Send connect command to SSH thread (non-blocking)
    if (!ssh_thread_connect(&ssh_thread_mgr, hostname, safe_port, username, password)) {
        ui_manager_show_connection_error("Failed to send connection request to SSH thread");
        return false;
    }

    // Mark as connecting - ssh_manager_poll_and_read() will handle progress
    app->ssh_connecting = true;
    return true;
}

bool ssh_manager_connect_negotiate(app_state_t* app, const char* hostname, int port,
                                  const char* username) {
    if (!app || !hostname || !username) {
        return false;
    }

    ui_manager_show_connecting_message();
    ui_manager_reset_auth_header();

    // Validate and set default port if needed
    int safe_port = (port > 0 && port <= 65535) ? port : 22;

    // Advance the generation so events from any previous (aborted) attempt are discarded.
    ssh_thread_mgr.connect_gen++;

    // Send connect command to SSH thread with empty password (auth will be negotiated)
    if (!ssh_thread_connect(&ssh_thread_mgr, hostname, safe_port, username, "")) {
        ui_manager_show_connection_error("Failed to send connection request to SSH thread");
        return false;
    }

    // Mark as connecting - ssh_manager_poll_and_read() will handle progress and auth prompts
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
    if (!ssh_thread_send_raw_input(&ssh_thread_mgr, data, len)) {
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
        // Discard events from a previous (cancelled/superseded) connection attempt.
        // gen == 0 means the event is not generation-tagged (e.g. data/disconnect
        // events from active I/O threads) and should always be processed.
        if (event.gen != 0 && event.gen != ssh_thread_mgr.connect_gen) {
            printf("SSH Manager: Discarding stale event type=%d (gen %u != current %u)\n",
                   event.type, event.gen, ssh_thread_mgr.connect_gen);
            continue;
        }

        switch (event.type) {
            case SSH_EVENT_CONNECTED: {
                printf("SSH Manager: Connection established\n");
                app->ssh_connected = true;
                app->ssh_connecting = false;
                app->had_ssh_session = true;

                // Set input mode to normal so keyboard input goes to SSH session
                app->input_mode = INPUT_MODE_NORMAL;

                ui_manager_show_connection_success(event.connected.hostname, event.connected.username);
                data_received = true;
                break;
            }

            case SSH_EVENT_CONNECTION_FAILED: {
                printf("SSH Manager: Connection failed: %s\n", event.error.message);
                ui_manager_show_connection_error(event.error.message);
                app->ssh_connecting = false;
                app->input_mode = INPUT_MODE_DISCONNECT_PROMPT;
                ui_manager_show_retry_prompt();
                break;
            }

            case SSH_EVENT_DATA_RECEIVED: {
                // Forward data to terminal
                term_input_bytes((const uint8_t*)event.data_received.data, event.data_received.len);
                data_received = true;
                break;
            }

            case SSH_EVENT_AUTH_PROMPT: {
                printf("SSH Manager: Authentication prompt received: %s\n", event.auth_prompt.prompt_text);
                app->input_mode = INPUT_MODE_AUTH_PROMPT;
                // Store prompt data in app_state so input_system can read it without
                // calling back into ssh_manager (keeps input_system domain-agnostic).
                strncpy(app->auth_prompt_text, event.auth_prompt.prompt_text,
                        sizeof(app->auth_prompt_text) - 1);
                app->auth_prompt_text[sizeof(app->auth_prompt_text) - 1] = '\0';
                app->auth_prompt_echo = event.auth_prompt.echo_input;
                // Clear any previous auth response
                memset(app->connection_input.auth_response, 0, sizeof(app->connection_input.auth_response));
                ui_manager_show_auth_prompt(event.auth_prompt.prompt_text,
                                            event.auth_prompt.echo_input);
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

bool ssh_manager_submit_auth_response(app_state_t* app, const char* response) {
    if (!app || !response) {
        return false;
    }

    // The SSH thread is blocked in the auth callback waiting for this response.
    // Route through ssh_thread to avoid reaching past its public interface.
    ssh_thread_submit_auth_response(&ssh_thread_mgr, response);

    return true;
}

// Helper function to handle disconnection with consistent cleanup
static void ssh_manager_handle_disconnect(app_state_t* app, const char* message, bool show_in_terminal) {
    if (!app || !message) {
        return;
    }

    // ssh_manager_cleanup() sends SSH_CMD_DISCONNECT which causes the SSH thread
    // to post a second SSH_EVENT_DISCONNECTED. Ignore it if we already cleaned up.
    if (!app->ssh_connected && !app->ssh_connecting) {
        return;
    }

    // Output to console
    printf("%s\n", message);

    if (show_in_terminal) {
        ui_manager_show_disconnect_message(message);
    }

    // Cleanup
    ssh_manager_cleanup(app);

    // Return to disconnect prompt so the user can get back to the main menu
    app->input_mode = INPUT_MODE_DISCONNECT_PROMPT;
    ui_manager_display_current_prompt(app);
}

bool ssh_manager_is_connecting(app_state_t* app) {
    return app && app->ssh_connecting;
}

void ssh_manager_cleanup(app_state_t* app) {
    if (!app) {
        return;
    }

    if (app->ssh_connected || app->ssh_connecting) {
        // Advance the generation BEFORE sending the disconnect command so that
        // any events the SSH thread posts after this point are immediately stale.
        ssh_thread_mgr.connect_gen++;
        ssh_thread_disconnect(&ssh_thread_mgr);
    }

    app->ssh_connected = false;
    app->ssh_connecting = false;
}

