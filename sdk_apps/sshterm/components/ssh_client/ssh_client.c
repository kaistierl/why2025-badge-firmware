#include "ssh_client.h"
#include "custom_io.h"
#include "user_settings.h"  /* wolfSSL/wolfSSH user configuration */

/* wolfSSH includes */
#include <wolfssh/ssh.h>
#include <wolfssh/error.h>
#include <wolfssh/settings.h>

/* wolfSSL includes */
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/wc_port.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/ssl.h>

/* Application includes */
#include "../common/terminal_config.h"  /* For SSH_TERMINAL_WIDTH/HEIGHT */

/* System includes */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

/* Compile-time verification - we need custom I/O for BadgeVMS */
#ifndef WOLFSSH_USER_IO
#error "WOLFSSH_USER_IO should be defined for BadgeVMS compatibility"
#endif

#ifndef WOLFSSH_TERM
#error "WOLFSSH_TERM should be defined in user_settings.h"
#endif

// Global state for authentication
static const char* stored_password = NULL;
static const char* stored_username = NULL;
static ssh_client_t* current_client = NULL;

// Thread synchronization for authentication prompts
static volatile bool auth_response_ready = false;
static char auth_response_buffer[512];
static volatile bool auth_input_needed = false;
static volatile bool auth_in_progress = false;

// Auth retry tracking
static int password_retry_count = 0;
#define MAX_PASSWORD_RETRIES 3

// Auth event callback
static auth_event_callback_t auth_event_callback = NULL;

// Authentication timeout (60 seconds)
#define AUTH_TIMEOUT_SEC 60

// Internal helper to set error message
static void ssh_set_error(ssh_client_t* client, const char* msg) {
    if (client && msg) {
        strncpy(client->error_msg, msg, sizeof(client->error_msg) - 1);
        client->error_msg[sizeof(client->error_msg) - 1] = '\0';
        client->state = SSH_STATE_ERROR;
        printf("SSH Error: %s\n", msg);
    }
}

void ssh_client_set_auth_event_callback(auth_event_callback_t callback) {
    auth_event_callback = callback;
}

bool ssh_client_needs_auth_input(void) {
    return auth_input_needed && auth_in_progress && !auth_response_ready;
}

const char* ssh_client_get_auth_prompt(void) {
    return (current_client != NULL) ? current_client->auth_prompt.prompt_text : "";
}

bool ssh_client_auth_prompt_echo(void) {
    return (current_client != NULL) ? current_client->auth_prompt.echo_input : false;
}

void ssh_client_submit_auth_response(const char* response) {
    if (!response) return;

    strncpy(auth_response_buffer, response, sizeof(auth_response_buffer) - 1);
    auth_response_buffer[sizeof(auth_response_buffer) - 1] = '\0';
    auth_response_ready = true;
}

// Public key check callback (accepting all for now - can be enhanced for security)
static int ssh_public_key_check(const byte* pubKey, word32 pubKeySz, void* ctx) {
    (void)pubKey;
    (void)pubKeySz;
    (void)ctx;

    // For now, accept all server keys (TODO: security enhancement needed for production)
    return WS_SUCCESS;
}

// Forward declarations for authentication helper functions
static int ssh_handle_password_auth(WS_UserAuthData* authData);
static int ssh_handle_keyboard_interactive_auth(WS_UserAuthData* authData);

// Enhanced authentication callback with keyboard-interactive support
static int ssh_auth_callback(byte authType, WS_UserAuthData* authData, void* ctx) {
    int ret = WOLFSSH_USERAUTH_FAILURE;

    printf("SSH Auth: Server supports auth types: ");
    if (authData->type & WOLFSSH_USERAUTH_PASSWORD) printf("password ");
    if (authData->type & WOLFSSH_USERAUTH_PUBLICKEY) printf("publickey ");
    if (authData->type & WOLFSSH_USERAUTH_KEYBOARD) printf("keyboard-interactive ");
    printf("\nSSH Auth: Attempting type %d\n", authType);

    switch (authType) {
        case WOLFSSH_USERAUTH_PASSWORD:
            ret = ssh_handle_password_auth(authData);
            break;

        case WOLFSSH_USERAUTH_KEYBOARD:
            ret = ssh_handle_keyboard_interactive_auth(authData);
            break;

        case WOLFSSH_USERAUTH_PUBLICKEY:
            printf("SSH Auth: Public key auth not implemented\n");
            ret = WOLFSSH_USERAUTH_FAILURE;
            break;

        default:
            printf("SSH Auth: Unsupported auth type: %d\n", authType);
            ret = WOLFSSH_USERAUTH_FAILURE;
            break;
    }

    (void)ctx;
    return ret;
}

// Handle password authentication
static int ssh_handle_password_auth(WS_UserAuthData* authData) {
    // Check retry limit for password auth
    if (password_retry_count >= MAX_PASSWORD_RETRIES) {
        printf("SSH Auth: Maximum password attempts (%d) exceeded\n", MAX_PASSWORD_RETRIES);
        return WOLFSSH_USERAUTH_FAILURE;
    }

    if (stored_password != NULL && strlen(stored_password) > 0) {
        password_retry_count++;
        authData->sf.password.password = (byte*)stored_password;
        authData->sf.password.passwordSz = (word32)strlen(stored_password);
        return WOLFSSH_USERAUTH_SUCCESS;
    }

    if (current_client == NULL) {
        return WOLFSSH_USERAUTH_FAILURE;
    }

    // Set up password prompt for the main thread to display
    current_client->auth_prompt.method = AUTH_METHOD_PASSWORD;
    strncpy(current_client->auth_prompt.prompt_text, "Password: ",
            sizeof(current_client->auth_prompt.prompt_text) - 1);
    current_client->auth_prompt.prompt_text[sizeof(current_client->auth_prompt.prompt_text) - 1] = '\0';
    current_client->auth_prompt.echo_input = false;
    auth_input_needed = true;
    auth_response_ready = false;
    auth_in_progress = true;

    // Immediately trigger auth event callback if set
    if (auth_event_callback) {
        auth_event_callback("Password: ", false, "password");
    }

    // Wait for response from main thread
    int timeout_count = 0;
    const int max_timeout = AUTH_TIMEOUT_SEC * 20; // 20 polls per second = 50ms each
    
    while (!auth_response_ready && auth_in_progress && timeout_count < max_timeout) {
        usleep(50000); // 50ms
        timeout_count++;
    }

    if (auth_response_ready) {
        // Use the response directly (password buffer must remain valid)
        authData->sf.password.password = (byte*)auth_response_buffer;
        authData->sf.password.passwordSz = (word32)strlen(auth_response_buffer);

        // Reset state but keep response buffer valid for wolfSSH
        auth_input_needed = false;
        auth_in_progress = false;
        // Note: We don't reset auth_response_ready here to keep buffer valid

        return WOLFSSH_USERAUTH_SUCCESS;
    } else {
        // Auth timed out or was cancelled
        printf("SSH Auth: Password prompt timed out after %d seconds\n", AUTH_TIMEOUT_SEC);
        auth_in_progress = false;
        auth_input_needed = false;
        return WOLFSSH_USERAUTH_FAILURE;
    }
}

// Handle keyboard-interactive authentication (blocking approach like wolfSSH example)
static int ssh_handle_keyboard_interactive_auth(WS_UserAuthData* authData) {
    if (current_client == NULL) {
        printf("SSH Auth: No current client for keyboard-interactive auth\n");
        return WOLFSSH_USERAUTH_FAILURE;
    }

    WS_UserAuthData_Keyboard* kb = &authData->sf.keyboard;

    // Display server name and instruction if provided
    if (kb->promptName && kb->promptName[0] != '\0') {
        printf("SSH Auth Name: %s\n", kb->promptName);
    }
    if (kb->promptInstruction && kb->promptInstruction[0] != '\0') {
        printf("SSH Auth Instruction: %s\n", kb->promptInstruction);
    }

    // Allocate response arrays (similar to wolfSSH example)
    kb->responses = (byte**)calloc(kb->promptCount, sizeof(byte*));
    kb->responseLengths = (word32*)calloc(kb->promptCount, sizeof(word32));

    if (!kb->responses || !kb->responseLengths) {
        printf("SSH Auth: Memory allocation failed for keyboard-interactive\n");
        if (kb->responses) free(kb->responses);
        if (kb->responseLengths) free(kb->responseLengths);
        return WOLFSSH_USERAUTH_FAILURE;
    }

    // Process each prompt (simplified polling until we get responses)
    for (word32 i = 0; i < kb->promptCount; i++) {
        // Set up auth prompt state for the main thread to display
        current_client->auth_prompt.method = AUTH_METHOD_KEYBOARD_INTERACTIVE;
        strncpy(current_client->auth_prompt.prompt_text, (char*)kb->prompts[i],
                sizeof(current_client->auth_prompt.prompt_text) - 1);
        current_client->auth_prompt.prompt_text[sizeof(current_client->auth_prompt.prompt_text) - 1] = '\0';
        current_client->auth_prompt.echo_input = kb->promptEcho[i];
        auth_input_needed = true;
        auth_response_ready = false;
        auth_in_progress = true;

        // Immediately trigger auth event callback if set
        if (auth_event_callback) {
            auth_event_callback((char*)kb->prompts[i], kb->promptEcho[i], "keyboard-interactive");
        }

        // Wait for response from main thread
        int timeout_count = 0;
        const int max_timeout = AUTH_TIMEOUT_SEC * 20; // 20 polls per second = 50ms each
        
        while (!auth_response_ready && auth_in_progress && timeout_count < max_timeout) {
            usleep(50000); // 50ms
            timeout_count++;
        }

        if (auth_response_ready) {
            // Copy the response (similar to wolfSSH example using strdup)
            kb->responses[i] = (byte*)strdup(auth_response_buffer);
            kb->responseLengths[i] = (word32)strlen(auth_response_buffer);
            kb->responseCount++;

            // Reset for next prompt
            auth_response_ready = false;
            auth_input_needed = false;
        } else {
            // Auth timed out or was cancelled
            printf("SSH Auth: Prompt %d timed out after %d seconds\n", i, AUTH_TIMEOUT_SEC);
            auth_in_progress = false;
            auth_input_needed = false;

            // Clean up allocated responses
            for (word32 j = 0; j < i; j++) {
                if (kb->responses[j]) free(kb->responses[j]);
            }
            free(kb->responses);
            free(kb->responseLengths);
            return WOLFSSH_USERAUTH_FAILURE;
        }
    }

    // All prompts handled successfully
    auth_in_progress = false;
    auth_input_needed = false;

    return WOLFSSH_USERAUTH_SUCCESS;
}

// Internal helper to create socket connection
static int ssh_create_socket(const char* hostname, int port) {
    struct addrinfo hints, *result;
    int sock_fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int ret = getaddrinfo(hostname, port_str, &hints, &result);
    if (ret != 0) {
        printf("SSH: Failed to resolve hostname %s:%d\n", hostname, port);
        return -1;
    }

    struct addrinfo* rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1) {
            continue;
        }

        int connect_result = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (connect_result == 0) {
            // Connection successful - keep socket in blocking mode
            break;
        } else {
            // Connection failed, try next address
            close(sock_fd);
            sock_fd = -1;
        }
    }

    freeaddrinfo(result);

    return sock_fd;
}

bool ssh_client_init(ssh_client_t* client) {
    if (!client) {
        return false;
    }

    printf("SSH: Initializing client subsystem\n");

    // Initialize wolfSSL/wolfCrypt first (required for wolfSSH)
    int wc_ret = wolfCrypt_Init();
    if (wc_ret != 0) {
        printf("SSH: wolfCrypt_Init failed with error: %d\n", wc_ret);
        ssh_set_error(client, "Failed to initialize wolfCrypt");
        return false;
    }

    // Test RNG functionality before proceeding
    WC_RNG rng;
    int rng_ret = wc_InitRng(&rng);
    if (rng_ret != 0) {
        printf("SSH: Failed to initialize RNG with error: %d\n", rng_ret);
        ssh_set_error(client, "Failed to initialize RNG");
        wolfCrypt_Cleanup();
        return false;
    }

    unsigned char test_random[16];
    rng_ret = wc_RNG_GenerateBlock(&rng, test_random, sizeof(test_random));
    if (rng_ret != 0) {
        printf("SSH: RNG test failed with error: %d\n", rng_ret);
        wc_FreeRng(&rng);
        ssh_set_error(client, "RNG functionality test failed");
        wolfCrypt_Cleanup();
        return false;
    }

    wc_FreeRng(&rng);

    // Initialize wolfSSH library
    int rc = wolfSSH_Init();
    if (rc != WS_SUCCESS) {
        printf("SSH: wolfSSH_Init failed with error: %d\n", rc);
        ssh_set_error(client, "Failed to initialize wolfSSH library");
        wolfCrypt_Cleanup();
        return false;
    }

    // Initialize client structure
    memset(client, 0, sizeof(ssh_client_t));
    client->state = SSH_STATE_DISCONNECTED;
    client->socket_fd = -1;

    return true;
}

// Internal helper to set up SSH session after socket is connected
static bool ssh_setup_session(ssh_client_t* client, const char* hostname, const char* username) {
    if (!client || !hostname || !username) {
        return false;
    }

    // Create wolfSSH context
    client->ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (!client->ctx) {
        ssh_set_error(client, "Failed to create SSH context");
        close(client->socket_fd);
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }

    // Configure context before creating sessions
    wolfSSH_SetUserAuth((WOLFSSH_CTX*)client->ctx, ssh_auth_callback);
    wolfSSH_CTX_SetPublicKeyCheck((WOLFSSH_CTX*)client->ctx, ssh_public_key_check);

    // Set up custom I/O callbacks on context
#ifdef WOLFSSH_USER_IO
    setup_wolfssh_custom_io((WOLFSSH_CTX*)client->ctx);
#endif

    // Create SSH session
    client->ssh = wolfSSH_new((WOLFSSH_CTX*)client->ctx);
    if (!client->ssh) {
        ssh_set_error(client, "Failed to create SSH session");
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        close(client->socket_fd);
        client->ctx = NULL;
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }

    // Set authentication context (password will be passed to callback)
    wolfSSH_SetUserAuthCtx((WOLFSSH*)client->ssh, (void*)client);

    // Set public key check context
    wolfSSH_SetPublicKeyCheckCtx((WOLFSSH*)client->ssh, (void*)hostname);

    // Set socket file descriptor
    int ret = wolfSSH_set_fd((WOLFSSH*)client->ssh, client->socket_fd);
    if (ret != WS_SUCCESS) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Failed to set socket for SSH session (error: %d)", ret);
        ssh_set_error(client, error_msg);
        wolfSSH_free((WOLFSSH*)client->ssh);
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        close(client->socket_fd);
        client->ssh = NULL;
        client->ctx = NULL;
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }

    // Set I/O context for custom I/O callbacks
#ifdef WOLFSSH_USER_IO
    wolfSSH_SetIOReadCtx((WOLFSSH*)client->ssh, &client->socket_fd);
    wolfSSH_SetIOWriteCtx((WOLFSSH*)client->ssh, &client->socket_fd);
#endif

    // Set username
    ret = wolfSSH_SetUsername((WOLFSSH*)client->ssh, username);
    if (ret != WS_SUCCESS) {
        ssh_set_error(client, "Failed to set username for SSH session");
        wolfSSH_free((WOLFSSH*)client->ssh);
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        close(client->socket_fd);
        client->ssh = NULL;
        client->ctx = NULL;
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }

    // Set channel type to terminal for interactive shell
    // Setup terminal channel type for interactive sessions
    ret = wolfSSH_SetChannelType((WOLFSSH*)client->ssh, WOLFSSH_SESSION_TERMINAL, NULL, 0);
    if (ret != WS_SUCCESS) {
        char error_details[256];
        snprintf(error_details, sizeof(error_details),
                "Failed to set terminal channel type (error code: %d)", ret);
        ssh_set_error(client, error_details);
        wolfSSH_free((WOLFSSH*)client->ssh);
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        close(client->socket_fd);
        client->ssh = NULL;
        client->ctx = NULL;
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }

    client->state = SSH_STATE_SSH_HANDSHAKING;
    return true;
}

bool ssh_client_connect_start(ssh_client_t* client, const char* hostname, int port,
                             const char* username, const char* password) {
    if (!client || !hostname || !username) {
        ssh_set_error(client, "Invalid parameters for SSH connection");
        return false;
    }

    printf("SSH: Connecting to %s:%d as %s\n", hostname, port, username);

    client->state = SSH_STATE_SOCKET_CONNECTING;

    // Store connection parameters
    strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    client->hostname[sizeof(client->hostname) - 1] = '\0';
    client->port = port;
    client->username = username;

    // Store credentials for authentication callback
    stored_password = password;  // Can be NULL for dynamic auth
    stored_username = username;

    // Reset auth retry counters for new connection
    password_retry_count = 0;

    // Set current client for authentication callbacks
    current_client = client;

    // Initialize auth prompt state
    memset(&client->auth_prompt, 0, sizeof(client->auth_prompt));

    // Reset auth state
    auth_response_ready = false;
    auth_input_needed = false;
    auth_in_progress = false;
    memset(auth_response_buffer, 0, sizeof(auth_response_buffer));

    // Create socket connection (blocking)
    client->socket_fd = ssh_create_socket(hostname, port);
    if (client->socket_fd == -1) {
        ssh_set_error(client, "Failed to create socket connection to host");
        client->state = SSH_STATE_ERROR;
        return false;
    }

    // Now set up the SSH session with the connected socket
    if (!ssh_setup_session(client, hostname, username)) {
        // Setup failed, state is already set to ERROR
        return false;
    }

    // Session setup successful, ready for SSH handshake
    client->state = SSH_STATE_SSH_HANDSHAKING;
    return true;
}

bool ssh_client_connect_continue(ssh_client_t* client) {
    if (!client) {
        return false;
    }

    switch (client->state) {
        case SSH_STATE_SSH_HANDSHAKING:
        case SSH_STATE_AUTHENTICATING: {
            // Attempt SSH handshake and authentication (BLOCKING)
            printf("SSH: Attempting connection handshake...\n");
            int ret = wolfSSH_connect((WOLFSSH*)client->ssh);

            // In blocking mode, wolfSSH_connect should complete immediately
            if (ret == WS_SUCCESS) {
                // Connection successful
                client->state = SSH_STATE_CONNECTED;
                printf("SSH: Connection established successfully\n");

                // Set terminal size to match our configured terminal emulator
                int pty_ret = wolfSSH_ChangeTerminalSize((WOLFSSH*)client->ssh,
                                                       TERMINAL_COLS, TERMINAL_ROWS, 0, 0);
                if (pty_ret != WS_SUCCESS) {
                    printf("SSH: Warning - failed to set terminal size (error: %d)\n", pty_ret);
                    // Don't fail the connection for this, just warn
                }

                return false; // Done - success
            } else {
                // Connection failed - get detailed error information
                const char* error_name = wolfSSH_get_error_name((WOLFSSH*)client->ssh);
                int error_code = wolfSSH_get_error((WOLFSSH*)client->ssh);

                printf("SSH: Connection failed with code %d (%s)\n", ret, error_name ? error_name : "unknown");
                printf("SSH: Additional error info: %d\n", error_code);

                char error_details[512];
                snprintf(error_details, sizeof(error_details),
                        "SSH connection failed (ret=%d, %s)", ret,
                        error_name ? error_name : "unknown error");
                ssh_set_error(client, error_details);

                wolfSSH_free((WOLFSSH*)client->ssh);
                wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
                close(client->socket_fd);
                client->ssh = NULL;
                client->ctx = NULL;
                client->socket_fd = -1;
                client->state = SSH_STATE_ERROR;
                return false; // Done - error
            }
        }

        case SSH_STATE_CONNECTED:
            return false; // Already connected, done

        case SSH_STATE_ERROR:
        case SSH_STATE_DISCONNECTED:
        default:
            return false; // Done (error or invalid state)
    }
}

bool ssh_client_send(ssh_client_t* client, const char* data, size_t len) {
    if (!client || !data || len == 0 || client->state != SSH_STATE_CONNECTED) {
        return false;
    }

    int bytes_written = wolfSSH_stream_send((WOLFSSH*)client->ssh, (byte*)data, (word32)len);

    if (bytes_written < 0) {
        ssh_set_error(client, "Failed to send data");
        return false;
    }

    return (bytes_written == (int)len);
}

int ssh_client_receive(ssh_client_t* client, char* buffer, int buffer_size) {
    if (!client || !buffer || buffer_size <= 0) {
        return -1;
    }

    if (client->state != SSH_STATE_CONNECTED || !client->ssh) {
        return -1;
    }

    // In pure blocking mode, just call wolfSSH_stream_read directly
    // It will block until data is available or an error occurs
    int bytes_read = wolfSSH_stream_read((WOLFSSH*)client->ssh, (byte*)buffer, (word32)buffer_size);

    if (bytes_read == WS_EOF) {
        snprintf(client->error_msg, sizeof(client->error_msg),
                "Connection to %s closed.", client->hostname);
        client->state = SSH_STATE_DISCONNECTED;
        return -2; // Special return code for clean disconnect
    } else if (bytes_read < 0) {
        // Check if it's a non-fatal error
        const char* error_name = wolfSSH_get_error_name((WOLFSSH*)client->ssh);
        if (error_name && (strstr(error_name, "WANT_READ") || strstr(error_name, "AGAIN"))) {
            // No data available - this shouldn't happen in blocking mode
            return 0;
        }

        char error_details[512];
        snprintf(error_details, sizeof(error_details),
                "Failed to receive data (ret=%d, %s)", bytes_read,
                error_name ? error_name : "unknown error");
        ssh_set_error(client, error_details);
        return -1;
    }

    return bytes_read;
}

bool ssh_client_resize_pty(ssh_client_t* client, int width, int height) {
    if (!client || client->state != SSH_STATE_CONNECTED || !client->ssh) {
        return false;
    }

    // Send terminal resize request to the remote server
    int ret = wolfSSH_ChangeTerminalSize((WOLFSSH*)client->ssh, width, height, 0, 0);
    if (ret != WS_SUCCESS) {
        return false;
    }

    return true;
}

bool ssh_client_send_signal(ssh_client_t* client, const char* signal_name) {
    if (!client || !signal_name || client->state != SSH_STATE_CONNECTED || !client->ssh) {
        return false;
    }

    // Signal sending is not implemented in this wolfSSH-based client
    // This would require implementing SSH_MSG_CHANNEL_REQUEST with signal type
    // Most terminal applications handle signals locally (Ctrl+C, etc.)
    (void)signal_name;
    return false;
}

bool ssh_client_get_exit_status(ssh_client_t* client, int* exit_status) {
    if (!client || !exit_status || !client->ssh) {
        return false;
    }

    // Exit status retrieval is not implemented in this wolfSSH-based client
    // This feature would require additional wolfSSH API support for channel exit status
    // Most interactive terminal sessions don't rely on programmatic exit status
    *exit_status = 0; // Default to success
    return false;
}

bool ssh_client_is_connected(ssh_client_t* client) {
    return client && client->state == SSH_STATE_CONNECTED;
}

ssh_state_t ssh_client_get_state(ssh_client_t* client) {
    return client ? client->state : SSH_STATE_ERROR;
}

const char* ssh_client_get_error(ssh_client_t* client) {
    return client ? client->error_msg : "Invalid client";
}

void ssh_client_cleanup(ssh_client_t* client) {
    if (!client) {
        return;
    }

    printf("SSH: Cleaning up connection\n");

    // Free SSH session
    if (client->ssh) {
        wolfSSH_shutdown((WOLFSSH*)client->ssh);
        wolfSSH_free((WOLFSSH*)client->ssh);
        client->ssh = NULL;
    }

    // Free SSH context
    if (client->ctx) {
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        client->ctx = NULL;
    }

    // Close socket
    if (client->socket_fd != -1) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    // Reset client state
    client->state = SSH_STATE_DISCONNECTED;
    memset(client->error_msg, 0, sizeof(client->error_msg));

    // Clear stored credentials
    stored_password = NULL;
    stored_username = NULL;

    // Clear current client reference
    if (current_client == client) {
        current_client = NULL;
    }
}

// New functions to support wolfSSH threading pattern

int ssh_client_get_fd(ssh_client_t* client) {
    if (!client) {
        return -1;
    }
    return client->socket_fd;
}

bool ssh_client_peek(ssh_client_t* client) {
    if (!client || !client->ssh) {
        return false;
    }

    // Use wolfSSH_stream_peek to check if data is available
    char temp_buf[1];
    int peek_result = wolfSSH_stream_peek((WOLFSSH*)client->ssh, (byte*)temp_buf, 1);

    return peek_result > 0;
}
