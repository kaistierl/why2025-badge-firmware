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

/* SDL3 for threading support */
#include <SDL3/SDL.h>
#include <SDL3/SDL_mutex.h>

/* System includes */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
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

// === SAFE STRING OPERATIONS ===

/**
 * Safe string copy with guaranteed null termination
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Size of destination buffer
 * @return true if string fit completely, false if truncated
 */
static bool strncpy_safe(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return false;
    }

    size_t src_len = strlen(src);
    bool fits_completely = (src_len < dest_size);

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';

    return fits_completely;
}

/**
 * Secure memory clearing for sensitive data
 * @param ptr Pointer to memory to clear
 * @param size Size of memory to clear
 */
static void secure_clear(void* ptr, size_t size) {
    if (ptr && size > 0) {
        memset(ptr, 0, size);
        // Compiler barrier to prevent optimization
        __asm__ __volatile__("" ::: "memory");
    }
}

/**
 * Validate client instance integrity
 * @param client SSH client instance to validate
 * @return true if valid, false if corrupted or invalid
 */
static bool ssh_client_validate(ssh_client_t* client) {
    return (client != NULL &&
            client->is_initialized &&
            client->magic_number == SSH_CLIENT_MAGIC_NUMBER);
}

// === ERROR HANDLING ===

// Internal helper to set error message with safe string handling
static void ssh_set_error(ssh_client_t* client, const char* msg) {
    if (!ssh_client_validate(client) || !msg) return;

    if (!strncpy_safe(client->error_msg, msg, sizeof(client->error_msg))) {
        printf("SSH Error: (message truncated) %s\n", client->error_msg);
    } else {
        printf("SSH Error: %s\n", msg);
    }
    client->state = SSH_STATE_ERROR;
}

// === AUTHENTICATION STATE MANAGEMENT ===

void ssh_client_set_auth_event_callback(ssh_client_t* client, auth_event_callback_t callback) {
    if (!ssh_client_validate(client)) return;

    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
    client->auth_state.auth_event_callback = (void*)callback;
    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
}

bool ssh_client_needs_auth_input(ssh_client_t* client) {
    if (!ssh_client_validate(client)) return false;

    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
    bool needs_input = client->auth_state.auth_input_needed &&
                      client->auth_state.auth_in_progress &&
                      !client->auth_state.auth_response_ready;
    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

    return needs_input;
}

const char* ssh_client_get_auth_prompt(ssh_client_t* client) {
    if (!ssh_client_validate(client)) return "";

    return client->auth_prompt.prompt_text;
}

bool ssh_client_auth_prompt_echo(ssh_client_t* client) {
    if (!ssh_client_validate(client)) return false;

    return client->auth_prompt.echo_input;
}

void ssh_client_submit_auth_response(ssh_client_t* client, const char* response) {
    if (!ssh_client_validate(client) || !response) {
        return;
    }

    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
    strncpy_safe(client->auth_state.auth_response_buffer, response,
                 sizeof(client->auth_state.auth_response_buffer));
    client->auth_state.auth_response_ready = true;
    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
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
static int ssh_handle_password_auth(ssh_client_t* client, WS_UserAuthData* authData);
static int ssh_handle_keyboard_interactive_auth(ssh_client_t* client, WS_UserAuthData* authData);

// Enhanced authentication callback with keyboard-interactive support
static int ssh_auth_callback(byte authType, WS_UserAuthData* authData, void* ctx) {
    int ret = WOLFSSH_USERAUTH_FAILURE;

    // Extract client instance from user auth context (ctx is the client instance we set)
    ssh_client_t* client = (ssh_client_t*)ctx;
    if (!ssh_client_validate(client)) {
        printf("SSH Auth: Invalid client instance in callback\n");
        return WOLFSSH_USERAUTH_FAILURE;
    }

    printf("SSH Auth: Server supports auth types: ");
    if (authData->type & WOLFSSH_USERAUTH_PASSWORD) printf("password ");
    if (authData->type & WOLFSSH_USERAUTH_PUBLICKEY) printf("publickey ");
    if (authData->type & WOLFSSH_USERAUTH_KEYBOARD) printf("keyboard-interactive ");
    printf("\nSSH Auth: Attempting %s\n",
           authType == WOLFSSH_USERAUTH_PASSWORD  ? "password" :
           authType == WOLFSSH_USERAUTH_PUBLICKEY ? "publickey" :
           authType == WOLFSSH_USERAUTH_KEYBOARD  ? "keyboard-interactive" : "unknown");

    switch (authType) {
        case WOLFSSH_USERAUTH_PASSWORD:
            ret = ssh_handle_password_auth(client, authData);
            break;

        case WOLFSSH_USERAUTH_KEYBOARD:
            ret = ssh_handle_keyboard_interactive_auth(client, authData);
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

    return ret;
}

// Handle password authentication using client instance
static int ssh_handle_password_auth(ssh_client_t* client, WS_UserAuthData* authData) {
    if (!ssh_client_validate(client)) {
        return WOLFSSH_USERAUTH_FAILURE;
    }

    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

    // Check retry limit for password auth
    if (client->auth_state.password_retry_count >= MAX_PASSWORD_RETRIES) {
        printf("SSH Auth: Maximum password attempts (%d) exceeded\n", MAX_PASSWORD_RETRIES);
        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        return WOLFSSH_USERAUTH_FAILURE;
    }

    // Check if we have a stored password to use
    if (strlen(client->auth_state.stored_password) > 0) {
        client->auth_state.password_retry_count++;
        authData->sf.password.password = (byte*)client->auth_state.stored_password;
        authData->sf.password.passwordSz = (word32)strlen(client->auth_state.stored_password);
        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        return WOLFSSH_USERAUTH_SUCCESS;
    }

    // Set up password prompt for the main thread to display
    client->auth_prompt.method = AUTH_METHOD_PASSWORD;
    strncpy_safe(client->auth_prompt.prompt_text, "Password: ",
                 sizeof(client->auth_prompt.prompt_text));
    client->auth_prompt.echo_input = false;
    client->auth_state.auth_input_needed = true;
    client->auth_state.auth_response_ready = false;
    client->auth_state.auth_in_progress = true;

    // Trigger auth event callback if set
    auth_event_callback_t callback = (auth_event_callback_t)client->auth_state.auth_event_callback;
    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

    if (callback) {
        callback(client, "Password: ", false, "password");
    }

    // Wait for response from main thread with timeout
    int timeout_count = 0;
    const int max_timeout = AUTH_TIMEOUT_SEC * 20; // 20 polls per second = 50ms each

    while (timeout_count < max_timeout) {
        SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        bool response_ready = client->auth_state.auth_response_ready;
        bool in_progress = client->auth_state.auth_in_progress;
        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

        if (response_ready || !in_progress) break;

        usleep(50000); // 50ms
        timeout_count++;
    }

    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
    if (client->auth_state.auth_response_ready) {
        // Use the response directly (password buffer must remain valid)
        authData->sf.password.password = (byte*)client->auth_state.auth_response_buffer;
        authData->sf.password.passwordSz = (word32)strlen(client->auth_state.auth_response_buffer);

        // Reset state but keep response buffer valid for wolfSSH
        client->auth_state.auth_input_needed = false;
        client->auth_state.auth_in_progress = false;
        // Note: We don't reset auth_response_ready here to keep buffer valid

        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        return WOLFSSH_USERAUTH_SUCCESS;
    } else {
        // Auth timed out or was cancelled
        printf("SSH Auth: Password prompt timed out after %d seconds\n", AUTH_TIMEOUT_SEC);
        client->auth_state.auth_in_progress = false;
        client->auth_state.auth_input_needed = false;
        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        return WOLFSSH_USERAUTH_FAILURE;
    }
}

// Handle keyboard-interactive authentication (blocking approach like wolfSSH example)
// Handle keyboard-interactive authentication using client instance
static int ssh_handle_keyboard_interactive_auth(ssh_client_t* client, WS_UserAuthData* authData) {
    if (!ssh_client_validate(client)) {
        printf("SSH Auth: No valid client for keyboard-interactive auth\n");
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

    // Process each prompt
    for (word32 i = 0; i < kb->promptCount; i++) {
        SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

        // Set up auth prompt state for the main thread to display
        client->auth_prompt.method = AUTH_METHOD_KEYBOARD_INTERACTIVE;
        strncpy_safe(client->auth_prompt.prompt_text, (char*)kb->prompts[i],
                     sizeof(client->auth_prompt.prompt_text));
        client->auth_prompt.echo_input = kb->promptEcho[i];
        client->auth_state.auth_input_needed = true;
        client->auth_state.auth_response_ready = false;
        client->auth_state.auth_in_progress = true;

        // Trigger auth event callback if set
        auth_event_callback_t callback = (auth_event_callback_t)client->auth_state.auth_event_callback;
        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

        printf("SSH Auth: Triggering callback for prompt %d: '%s' (callback=%p)\n",
               i, kb->prompts[i] ? (char*)kb->prompts[i] : "(null)", (void*)callback);

        if (callback) {
            callback(client, (char*)kb->prompts[i], kb->promptEcho[i], "keyboard-interactive");
        } else {
            printf("SSH Auth: No callback set - prompt will timeout\n");
        }

        // Wait for response from main thread with timeout
        int timeout_count = 0;
        const int max_timeout = AUTH_TIMEOUT_SEC * 20; // 20 polls per second = 50ms each

        while (timeout_count < max_timeout) {
            SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
            bool response_ready = client->auth_state.auth_response_ready;
            bool in_progress = client->auth_state.auth_in_progress;
            SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

            if (response_ready || !in_progress) break;

            usleep(50000); // 50ms
            timeout_count++;
        }

        SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        if (client->auth_state.auth_response_ready) {
            // Copy the response (similar to wolfSSH example using strdup)
            kb->responses[i] = (byte*)strdup(client->auth_state.auth_response_buffer);
            kb->responseLengths[i] = (word32)strlen(client->auth_state.auth_response_buffer);
            kb->responseCount++;

            // Reset for next prompt
            client->auth_state.auth_response_ready = false;
            client->auth_state.auth_input_needed = false;
            SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        } else {
            // Auth timed out or was cancelled
            printf("SSH Auth: Prompt %d timed out after %d seconds\n", i, AUTH_TIMEOUT_SEC);
            client->auth_state.auth_in_progress = false;
            client->auth_state.auth_input_needed = false;
            SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

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
    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
    client->auth_state.auth_in_progress = false;
    client->auth_state.auth_input_needed = false;
    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

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
            break;
        } else {
            // Connection failed or timed out, try next address
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

    // Initialize client structure first
    memset(client, 0, sizeof(ssh_client_t));

    // Initialize memory safety fields
    client->magic_number = SSH_CLIENT_MAGIC_NUMBER;
    client->is_initialized = false;  // Set to true only after complete initialization
    client->state = SSH_STATE_DISCONNECTED;
    client->socket_fd = -1;

    // Initialize authentication state
    secure_clear(client->auth_state.stored_password, sizeof(client->auth_state.stored_password));
    secure_clear(client->auth_state.stored_username, sizeof(client->auth_state.stored_username));
    secure_clear(client->auth_state.auth_response_buffer, sizeof(client->auth_state.auth_response_buffer));
    client->auth_state.auth_response_ready = false;
    client->auth_state.auth_input_needed = false;
    client->auth_state.auth_in_progress = false;
    client->auth_state.password_retry_count = 0;
    client->auth_state.auth_event_callback = NULL;

    // Create authentication state mutex
    client->auth_state.auth_state_mutex = SDL_CreateMutex();
    if (!client->auth_state.auth_state_mutex) {
        printf("SSH: Failed to create authentication state mutex\n");
        ssh_set_error(client, "Failed to create mutex for thread safety");
        return false;
    }

    // Initialize wolfSSL/wolfCrypt first (required for wolfSSH)
    int wc_ret = wolfCrypt_Init();
    if (wc_ret != 0) {
        printf("SSH: wolfCrypt_Init failed with error: %d\n", wc_ret);
        ssh_set_error(client, "Failed to initialize wolfCrypt");
        SDL_DestroyMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        client->auth_state.auth_state_mutex = NULL;
        return false;
    }

    // Test RNG functionality before proceeding
    WC_RNG rng;
    int rng_ret = wc_InitRng(&rng);
    if (rng_ret != 0) {
        printf("SSH: Failed to initialize RNG with error: %d\n", rng_ret);
        ssh_set_error(client, "Failed to initialize RNG");
        SDL_DestroyMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        client->auth_state.auth_state_mutex = NULL;
        wolfCrypt_Cleanup();
        return false;
    }

    unsigned char test_random[16];
    rng_ret = wc_RNG_GenerateBlock(&rng, test_random, sizeof(test_random));
    if (rng_ret != 0) {
        printf("SSH: RNG test failed with error: %d\n", rng_ret);
        wc_FreeRng(&rng);
        ssh_set_error(client, "RNG functionality test failed");
        SDL_DestroyMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        client->auth_state.auth_state_mutex = NULL;
        wolfCrypt_Cleanup();
        return false;
    }

    wc_FreeRng(&rng);

    // Initialize wolfSSH library
    int rc = wolfSSH_Init();
    if (rc != WS_SUCCESS) {
        printf("SSH: wolfSSH_Init failed with error: %d\n", rc);
        ssh_set_error(client, "Failed to initialize wolfSSH library");
        SDL_DestroyMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        client->auth_state.auth_state_mutex = NULL;
        wolfCrypt_Cleanup();
        return false;
    }

    // Initialization successful - mark as initialized
    client->is_initialized = true;

    printf("SSH: Client initialization completed successfully\n");
    return true;
}

// Free wolfSSH session, context and socket — used on every ssh_setup_session error path
static void ssh_session_teardown(ssh_client_t* client) {
    if (client->ssh) {
        wolfSSH_free((WOLFSSH*)client->ssh);
        client->ssh = NULL;
    }
    if (client->ctx) {
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        client->ctx = NULL;
    }
    if (client->socket_fd != -1) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    client->state = SSH_STATE_ERROR;
}

// Internal helper to set up SSH session after socket is connected
static bool ssh_setup_session(ssh_client_t* client, const char* hostname, const char* username) {
    if (!client || !hostname || !username) {
        return false;
    }

    client->ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (!client->ctx) {
        ssh_set_error(client, "Failed to create SSH context");
        ssh_session_teardown(client);
        return false;
    }

    wolfSSH_SetUserAuth((WOLFSSH_CTX*)client->ctx, ssh_auth_callback);
    wolfSSH_CTX_SetPublicKeyCheck((WOLFSSH_CTX*)client->ctx, ssh_public_key_check);

#ifdef WOLFSSH_USER_IO
    setup_wolfssh_custom_io((WOLFSSH_CTX*)client->ctx);
#endif

    client->ssh = wolfSSH_new((WOLFSSH_CTX*)client->ctx);
    if (!client->ssh) {
        ssh_set_error(client, "Failed to create SSH session");
        ssh_session_teardown(client);
        return false;
    }

    wolfSSH_SetUserAuthCtx((WOLFSSH*)client->ssh, (void*)client);
    wolfSSH_SetPublicKeyCheckCtx((WOLFSSH*)client->ssh, (void*)hostname);

    int ret = wolfSSH_set_fd((WOLFSSH*)client->ssh, client->socket_fd);
    if (ret != WS_SUCCESS) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Failed to set socket for SSH session (error: %d)", ret);
        ssh_set_error(client, error_msg);
        ssh_session_teardown(client);
        return false;
    }

#ifdef WOLFSSH_USER_IO
    wolfSSH_SetIOReadCtx((WOLFSSH*)client->ssh, &client->socket_fd);
    wolfSSH_SetIOWriteCtx((WOLFSSH*)client->ssh, &client->socket_fd);
#endif

    ret = wolfSSH_SetUsername((WOLFSSH*)client->ssh, username);
    if (ret != WS_SUCCESS) {
        ssh_set_error(client, "Failed to set username for SSH session");
        ssh_session_teardown(client);
        return false;
    }

    ret = wolfSSH_SetChannelType((WOLFSSH*)client->ssh, WOLFSSH_SESSION_TERMINAL, NULL, 0);
    if (ret != WS_SUCCESS) {
        char error_details[256];
        snprintf(error_details, sizeof(error_details),
                "Failed to set terminal channel type (error code: %d)", ret);
        ssh_set_error(client, error_details);
        ssh_session_teardown(client);
        return false;
    }

    client->state = SSH_STATE_SSH_HANDSHAKING;
    return true;
}

bool ssh_client_connect_start(ssh_client_t* client, const char* hostname, int port,
                             const char* username, const char* password) {
    if (!ssh_client_validate(client) || !hostname || !username) {
        ssh_set_error(client, "Invalid parameters for SSH connection");
        return false;
    }

    printf("SSH: Connecting to %s:%d as %s\n", hostname, port, username);

    client->state = SSH_STATE_SOCKET_CONNECTING;

    // Store connection parameters using safe string operations
    strncpy_safe(client->hostname, hostname, sizeof(client->hostname));
    client->port = port;

    // Store credentials for authentication callback in client instance
    SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

    // Clear any previous credentials securely
    secure_clear(client->auth_state.stored_password, sizeof(client->auth_state.stored_password));
    secure_clear(client->auth_state.stored_username, sizeof(client->auth_state.stored_username));

    // Store new credentials if provided
    if (password) {
        strncpy_safe(client->auth_state.stored_password, password,
                     sizeof(client->auth_state.stored_password));
    }
    strncpy_safe(client->auth_state.stored_username, username,
                 sizeof(client->auth_state.stored_username));

    // Point username at the owned copy so it doesn't dangle after the caller's buffer is gone
    client->username = client->auth_state.stored_username;

    // Reset auth retry counters for new connection
    client->auth_state.password_retry_count = 0;

    // Initialize auth prompt state
    memset(&client->auth_prompt, 0, sizeof(client->auth_prompt));

    // Reset auth state
    client->auth_state.auth_response_ready = false;
    client->auth_state.auth_input_needed = false;
    client->auth_state.auth_in_progress = false;
    secure_clear(client->auth_state.auth_response_buffer, sizeof(client->auth_state.auth_response_buffer));

    SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

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

    // Attempt SSH handshake and authentication (blocking)
    printf("SSH: Attempting connection handshake...\n");
    int ret = wolfSSH_connect((WOLFSSH*)client->ssh);

    if (ret == WS_SUCCESS) {
        client->state = SSH_STATE_CONNECTED;
        printf("SSH: Connection established successfully\n");

        int pty_ret = wolfSSH_ChangeTerminalSize((WOLFSSH*)client->ssh,
                                               TERMINAL_COLS, TERMINAL_ROWS, 0, 0);
        if (pty_ret != WS_SUCCESS) {
            printf("SSH: Warning - failed to set terminal size (error: %d)\n", pty_ret);
        }
        return true;
    }

    const char* error_name = wolfSSH_get_error_name((WOLFSSH*)client->ssh);
    int error_code = wolfSSH_get_error((WOLFSSH*)client->ssh);
    printf("SSH: Connection failed with code %d (%s)\n", ret, error_name ? error_name : "unknown");
    printf("SSH: Additional error info: %d\n", error_code);

    char error_details[512];
    snprintf(error_details, sizeof(error_details),
            "SSH connection failed (ret=%d, %s)", ret,
            error_name ? error_name : "unknown error");
    ssh_set_error(client, error_details);
    ssh_session_teardown(client);
    return false;
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

    // Securely clear authentication state
    if (client->auth_state.auth_state_mutex) {
        SDL_LockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

        secure_clear(client->auth_state.stored_password, sizeof(client->auth_state.stored_password));
        secure_clear(client->auth_state.stored_username, sizeof(client->auth_state.stored_username));
        secure_clear(client->auth_state.auth_response_buffer, sizeof(client->auth_state.auth_response_buffer));
        client->auth_state.auth_response_ready = false;
        client->auth_state.auth_input_needed = false;
        client->auth_state.auth_in_progress = false;
        client->auth_state.password_retry_count = 0;
        client->auth_state.auth_event_callback = NULL;

        SDL_UnlockMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);

        // Destroy the mutex
        SDL_DestroyMutex((SDL_Mutex*)client->auth_state.auth_state_mutex);
        client->auth_state.auth_state_mutex = NULL;
    }

    // Reset client state
    client->state = SSH_STATE_DISCONNECTED;
    secure_clear(client->error_msg, sizeof(client->error_msg));

    // Clear initialization markers
    client->is_initialized = false;
    client->magic_number = 0;

    printf("SSH: Cleanup completed\n");
}

// New functions to support wolfSSH threading pattern

int ssh_client_get_fd(ssh_client_t* client) {
    if (!client) {
        return -1;
    }
    return client->socket_fd;
}

