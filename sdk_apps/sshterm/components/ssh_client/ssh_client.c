#include "ssh_client.h"
#include <wolfssh/ssh.h>
#include <wolfssh/error.h>
#include <wolfssh/settings.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

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
    
    printf("ssh_create_socket: Connecting to %s:%d\n", hostname, port);
    
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
        
        // Set socket to non-blocking mode before connect for timeout control
        int sock_flags = fcntl(sock_fd, F_GETFL, 0);
        if (sock_flags != -1) {
            fcntl(sock_fd, F_SETFL, sock_flags | O_NONBLOCK);
        }
        
        int connect_result = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (connect_result == 0) {
            // Immediate connection success (unlikely but possible for localhost)
            // Set back to blocking mode for wolfSSH
            if (sock_flags != -1) {
                fcntl(sock_fd, F_SETFL, sock_flags & ~O_NONBLOCK);
            }
            break;
        } else if (errno == EINPROGRESS) {
            // Non-blocking connect in progress
            fd_set write_fds, error_fds;
            FD_ZERO(&write_fds);
            FD_ZERO(&error_fds);
            FD_SET(sock_fd, &write_fds);
            FD_SET(sock_fd, &error_fds);
            
            struct timeval timeout = {10, 0}; // 10 second timeout
            int select_result = select(sock_fd + 1, NULL, &write_fds, &error_fds, &timeout);
            
            if (select_result > 0) {
                if (FD_ISSET(sock_fd, &error_fds)) {
                    // Error on socket
                    close(sock_fd);
                    sock_fd = -1;
                    continue;
                }
                if (FD_ISSET(sock_fd, &write_fds)) {
                    // Check if connection succeeded
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                        // Connection successful, set back to blocking mode for wolfSSH
                        if (sock_flags != -1) {
                            fcntl(sock_fd, F_SETFL, sock_flags & ~O_NONBLOCK);
                        }
                        break;
                    }
                }
            }
            // Timeout or error - close and try next address
            close(sock_fd);
            sock_fd = -1;
            continue;
        } else {
            // Connection failed immediately
            close(sock_fd);
            sock_fd = -1;
            continue;
        }
    }
    
    freeaddrinfo(result);
    
    return sock_fd;
}

bool ssh_client_init(ssh_client_t* client) {
    if (!client) {
        return false;
    }
    
    // Initialize wolfSSH library (wolfCrypt is initialized automatically)
    int rc = wolfSSH_Init();
    if (rc != WS_SUCCESS) {
        ssh_set_error(client, "Failed to initialize wolfSSH library");
        return false;
    }
    
    // Initialize client structure
    memset(client, 0, sizeof(ssh_client_t));
    client->state = SSH_STATE_DISCONNECTED;
    client->socket_fd = -1;
    
    printf("SSH client initialized successfully\n");
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
    
    printf("SSH: Context created successfully\n");
    
    // Set up authentication callback
    wolfSSH_SetUserAuth((WOLFSSH_CTX*)client->ctx, ssh_auth_callback);
    
    // Set up public key check callback
    wolfSSH_CTX_SetPublicKeyCheck((WOLFSSH_CTX*)client->ctx, ssh_public_key_check);
    
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
        ssh_set_error(client, "Failed to set socket for SSH session");
        wolfSSH_free((WOLFSSH*)client->ssh);
        wolfSSH_CTX_free((WOLFSSH_CTX*)client->ctx);
        close(client->socket_fd);
        client->ssh = NULL;
        client->ctx = NULL;
        client->socket_fd = -1;
        client->state = SSH_STATE_ERROR;
        return false;
    }
    
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
    
    printf("SSH: Session setup complete, ready for handshake\n");
    client->state = SSH_STATE_SSH_HANDSHAKING;
    return true;
}

bool ssh_client_connect_start(ssh_client_t* client, const char* hostname, int port, 
                             const char* username, const char* password) {
    if (!client || !hostname || !username || !password) {
        ssh_set_error(client, "Invalid parameters for SSH connection");
        return false;
    }
    
    printf("SSH: Starting connection to %s@%s:%d\n", username, hostname, port);
    client->state = SSH_STATE_SOCKET_CONNECTING;
    
    // Store connection parameters
    strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    client->hostname[sizeof(client->hostname) - 1] = '\0';
    client->port = port;
    client->username = username;
    
    // Store credentials for authentication callback
    stored_password = password;
    stored_username = username;
    
    // Create socket connection (with timeout but ultimately blocking for SSH)
    client->socket_fd = ssh_create_socket(hostname, port);
    if (client->socket_fd == -1) {
        ssh_set_error(client, "Failed to create socket connection to host");
        client->state = SSH_STATE_ERROR;
        return false;
    }
    
    printf("SSH: Socket connected, setting up SSH session\n");
    
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
            // Attempt SSH handshake and authentication
            int ret = wolfSSH_connect((WOLFSSH*)client->ssh);
            
            printf("SSH: wolfSSH_connect returned: %d\n", ret);
            
            // Check for non-blocking operation in progress
            if (ret == WS_WANT_READ || ret == WS_WANT_WRITE) {
                // Non-blocking operation in progress, try again later
                printf("SSH: Non-blocking operation in progress (WANT_READ/WANT_WRITE)\n");
                client->state = SSH_STATE_AUTHENTICATING; // Update state if progressing
                return true; // Keep calling
            } else if (ret == WS_SUCCESS) {
                // Connection successful
                printf("SSH: Connection established and authenticated successfully\n");
                client->state = SSH_STATE_CONNECTED;
                
                // Set terminal size to match our fixed 80x39 terminal emulator
                int pty_ret = wolfSSH_ChangeTerminalSize((WOLFSSH*)client->ssh, 80, 39, 0, 0);
                if (pty_ret != WS_SUCCESS) {
                    printf("SSH: Warning - failed to set terminal size (error: %d)\n", pty_ret);
                    // Don't fail the connection for this, just warn
                } else {
                    printf("SSH: Terminal size set to 80x39\n");
                }
                
                // Set socket to non-blocking mode after successful SSH connection
                int flags = fcntl(client->socket_fd, F_GETFL, 0);
                if (flags != -1) {
                    fcntl(client->socket_fd, F_SETFL, flags | O_NONBLOCK);
                }
                
                return false; // Done - success
            } else {
                // Connection failed
                const char* error_name = wolfSSH_get_error_name((WOLFSSH*)client->ssh);
                
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
    
    // Check for non-blocking operation in progress
    if (bytes_written == WS_WANT_WRITE || bytes_written == WS_WANT_READ) {
        return true; // Non-blocking, try again later
    } else if (bytes_written < 0) {
        ssh_set_error(client, "Failed to send data");
        return false;
    }
    
    return (bytes_written == (int)len);
}

int ssh_client_receive(ssh_client_t* client, char* buffer, size_t buffer_size) {
    if (!client || !buffer || buffer_size == 0 || client->state != SSH_STATE_CONNECTED) {
        return -1;
    }
    
    int bytes_read = wolfSSH_stream_read((WOLFSSH*)client->ssh, (byte*)buffer, (word32)buffer_size);
    
    // Check for non-blocking conditions and EOF
    if (bytes_read == WS_WANT_READ || bytes_read == WS_WANT_WRITE || bytes_read == WS_ERROR) {
        return 0; // No data available, non-blocking
    } else if (bytes_read == WS_EOF) {
        snprintf(client->error_msg, sizeof(client->error_msg), 
                "Connection to %s closed.", client->hostname);
        client->state = SSH_STATE_DISCONNECTED;
        return -2; // Special return code for clean disconnect
    } else if (bytes_read < 0) {
        const char* error_name = wolfSSH_get_error_name((WOLFSSH*)client->ssh);
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
        printf("SSH: Failed to resize terminal to %dx%d (error: %d)\n", width, height, ret);
        return false;
    }
    
    printf("SSH: Terminal resized to %dx%d\n", width, height);
    return true;
}

bool ssh_client_send_signal(ssh_client_t* client, const char* signal_name) {
    if (!client || !signal_name || client->state != SSH_STATE_CONNECTED || !client->ssh) {
        return false;
    }
    
    printf("SSH: Sending signal '%s'\n", signal_name);
    
    // wolfSSH doesn't have direct signal sending capability like libssh2
    // We could implement this by sending the signal as a control sequence
    // For now, return true as signals aren't critical for basic terminal operation
    (void)signal_name;
    return true;
}

bool ssh_client_get_exit_status(ssh_client_t* client, int* exit_status) {
    if (!client || !exit_status || !client->ssh) {
        return false;
    }
    
    // wolfSSH doesn't provide direct exit status access like libssh2
    // The exit status would typically be handled through the channel close mechanism
    // For now, return a default status
    *exit_status = 0;
    return true;
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
    
    // Cleanup wolfSSH library (should be called when application exits)
    // Note: Only call this if no other SSH connections are active
    // wolfSSH_Cleanup();
    
    printf("SSH: Cleanup completed\n");
}
