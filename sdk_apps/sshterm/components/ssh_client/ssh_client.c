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

// Internal helper to set error message
static void ssh_set_error(ssh_client_t* client, const char* msg) {
    if (client && msg) {
        strncpy(client->error_msg, msg, sizeof(client->error_msg) - 1);
        client->error_msg[sizeof(client->error_msg) - 1] = '\0';
        client->state = SSH_STATE_ERROR;
        printf("SSH Error: %s\n", msg);
    }
}

// Public key check callback (accepting all for now - can be enhanced for security)
static int ssh_public_key_check(const byte* pubKey, word32 pubKeySz, void* ctx) {
    (void)pubKey;
    (void)pubKeySz;
    (void)ctx;
    
    // For now, accept all server keys (TODO: security enhancement needed for production)
    return WS_SUCCESS;
}

// Comprehensive authentication callback based on wolfSSH examples
static int ssh_auth_callback(byte authType, WS_UserAuthData* authData, void* ctx) {
    int ret = WOLFSSH_USERAUTH_FAILURE;

    switch (authType) {
        case WOLFSSH_USERAUTH_PASSWORD:
            if (stored_password != NULL) {
                authData->sf.password.password = (byte*)stored_password;
                authData->sf.password.passwordSz = (word32)strlen(stored_password);
                ret = WOLFSSH_USERAUTH_SUCCESS;
            } else {
                printf("SSH: No password provided for authentication\n");
            }
            break;
            
        case WOLFSSH_USERAUTH_PUBLICKEY:
            // For now, we don't support public key auth but could add it later
            printf("SSH Auth: Public key auth not implemented\n");
            ret = WOLFSSH_USERAUTH_FAILURE;
            break;
            
        default:
            printf("SSH Auth: Unsupported auth type: %d\n", authType);
            ret = WOLFSSH_USERAUTH_FAILURE;
            break;
    }
    
    // Use ctx parameter to avoid unused variable warning
    (void)ctx;
    
    return ret;
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
    if (!client || !hostname || !username || !password) {
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
    stored_password = password;
    stored_username = username;
    
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
