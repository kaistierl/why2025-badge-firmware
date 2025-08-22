#include "ssh_client.h"
#include <libssh2.h>
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

// Internal helper to set error message
static void ssh_set_error(ssh_client_t* client, const char* msg) {
    if (client && msg) {
        strncpy(client->error_msg, msg, sizeof(client->error_msg) - 1);
        client->error_msg[sizeof(client->error_msg) - 1] = '\0';
        client->state = SSH_STATE_ERROR;
    }
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
        printf("getaddrinfo failed for %s:%d - error code: %d\n", hostname, port, ret);
        return -1;
    }
    
    printf("getaddrinfo succeeded, trying to connect...\n");
    
    struct addrinfo* rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1) {
            printf("socket() failed: %s\n", strerror(errno));
            continue;
        }
        
        printf("Socket created, attempting connect...\n");
        if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            printf("Connected successfully!\n");
            break; // Success
        }
        
        printf("connect() failed: %s\n", strerror(errno));
        close(sock_fd);
        sock_fd = -1;
    }
    
    freeaddrinfo(result);
    
    if (sock_fd != -1) {
        // Set socket to non-blocking mode
        int flags = fcntl(sock_fd, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    
    return sock_fd;
}

bool ssh_client_init(ssh_client_t* client) {
    if (!client) {
        return false;
    }
    
    // Initialize libssh2
    int rc = libssh2_init(0);
    if (rc != 0) {
        ssh_set_error(client, "Failed to initialize libssh2");
        return false;
    }
    
    // Initialize client structure
    memset(client, 0, sizeof(ssh_client_t));
    client->state = SSH_STATE_DISCONNECTED;
    client->socket_fd = -1;
    
    return true;
}

bool ssh_client_connect(ssh_client_t* client, const char* hostname, int port, 
                       const char* username, const char* password) {
    if (!client || !hostname || !username || !password) {
        ssh_set_error(client, "Invalid parameters");
        return false;
    }
    
    client->state = SSH_STATE_CONNECTING;
    strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    client->hostname[sizeof(client->hostname) - 1] = '\0';  // Ensure null termination
    client->port = port;
    client->username = username;
    
    // Create socket connection
    client->socket_fd = ssh_create_socket(hostname, port);
    if (client->socket_fd == -1) {
        ssh_set_error(client, "Failed to connect to host");
        return false;
    }
    
    // Create SSH session
    client->session = libssh2_session_init();
    if (!client->session) {
        ssh_set_error(client, "Failed to create SSH session");
        close(client->socket_fd);
        client->socket_fd = -1;
        return false;
    }
    
    // Set non-blocking mode
    libssh2_session_set_blocking((LIBSSH2_SESSION*)client->session, 0);
    
    // Perform SSH handshake
    int rc;
    while ((rc = libssh2_session_handshake((LIBSSH2_SESSION*)client->session, client->socket_fd)) == LIBSSH2_ERROR_EAGAIN);
    
    if (rc) {
        ssh_set_error(client, "Failed SSH handshake");
        libssh2_session_free((LIBSSH2_SESSION*)client->session);
        close(client->socket_fd);
        client->session = NULL;
        client->socket_fd = -1;
        return false;
    }
    
    client->state = SSH_STATE_AUTHENTICATING;
    
    // Authenticate with password
    while ((rc = libssh2_userauth_password((LIBSSH2_SESSION*)client->session, username, password)) == LIBSSH2_ERROR_EAGAIN);
    
    if (rc) {
        char error_details[512];
        char* libssh2_error_msg;
        int libssh2_error_len;
        int libssh2_error_code = libssh2_session_last_error((LIBSSH2_SESSION*)client->session, &libssh2_error_msg, &libssh2_error_len, 0);
        
        snprintf(error_details, sizeof(error_details), "Authentication failed (rc: %d, libssh2_code: %d, msg: %s)", 
                 rc, libssh2_error_code, libssh2_error_msg ? libssh2_error_msg : "unknown");
        ssh_set_error(client, error_details);
        
        libssh2_session_disconnect((LIBSSH2_SESSION*)client->session, "Authentication failed");
        libssh2_session_free((LIBSSH2_SESSION*)client->session);
        close(client->socket_fd);
        client->session = NULL;
        client->socket_fd = -1;
        return false;
    }
    
    // Open a channel for shell
    while (!(client->channel = libssh2_channel_open_session((LIBSSH2_SESSION*)client->session))) {
        int last_error = libssh2_session_last_errno((LIBSSH2_SESSION*)client->session);
        if (last_error != LIBSSH2_ERROR_EAGAIN) {
            ssh_set_error(client, "Failed to open channel");
            libssh2_session_disconnect((LIBSSH2_SESSION*)client->session, "Channel failed");
            libssh2_session_free((LIBSSH2_SESSION*)client->session);
            close(client->socket_fd);
            client->session = NULL;
            client->socket_fd = -1;
            return false;
        }
    }
    
    // Standard SSH shell startup: try PTY first, fallback to exec
    
    // Try PTY + shell (preferred for interactive sessions)
    // Use xterm-256color for better color support and proper dimensions: 80 columns x 39 rows
    const char* term_type = "xterm-256color";
    while ((rc = libssh2_channel_request_pty_ex((LIBSSH2_CHANNEL*)client->channel, term_type, strlen(term_type), NULL, 0, 80, 39, 0, 0)) == LIBSSH2_ERROR_EAGAIN);
    
    if (rc == 0) {
        printf("PTY allocated successfully with xterm-256color\n");
        
        // Request interactive shell on PTY immediately after allocation
        // Don't set environment variables on PTY channels - many servers reject this
        while ((rc = libssh2_channel_shell((LIBSSH2_CHANNEL*)client->channel)) == LIBSSH2_ERROR_EAGAIN);
        if (rc == 0) {
            printf("Interactive shell started on PTY\n");
            client->state = SSH_STATE_CONNECTED;
            return true;
        }
        
        printf("Shell request on PTY failed (code: %d), trying exec fallback...\n", rc);
    } else {
        printf("PTY allocation failed (code: %d), trying exec fallback...\n", rc);
    }
    
    // Fallback: close current channel and try exec shell without PTY
    libssh2_channel_close((LIBSSH2_CHANNEL*)client->channel);
    libssh2_channel_free((LIBSSH2_CHANNEL*)client->channel);
    
    // Open new channel for exec
    while (!(client->channel = libssh2_channel_open_session((LIBSSH2_SESSION*)client->session))) {
        int last_error = libssh2_session_last_errno((LIBSSH2_SESSION*)client->session);
        if (last_error != LIBSSH2_ERROR_EAGAIN) {
            ssh_set_error(client, "Failed to open exec channel");
            return false;
        }
    }
    
    printf("Exec channel opened, trying shells...\n");
    
    // Set environment variables for non-PTY connection to enable colors and features
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "TERM", "xterm-256color");
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "COLORTERM", "truecolor");
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "LC_ALL", "en_US.UTF-8");
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "LANG", "en_US.UTF-8");
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "COLUMNS", "80");
    libssh2_channel_setenv((LIBSSH2_CHANNEL*)client->channel, "LINES", "39");
    
    // Try exec shell (standard approach for non-PTY)
    while ((rc = libssh2_channel_exec((LIBSSH2_CHANNEL*)client->channel, "/bin/bash -l")) == LIBSSH2_ERROR_EAGAIN);
    if (rc == 0) {
        printf("Connected with /bin/bash -l\n");
    } else {
        printf("bash -l failed (code: %d), trying basic bash...\n", rc);
        
        // Close and reopen channel for next attempt
        libssh2_channel_close((LIBSSH2_CHANNEL*)client->channel);
        libssh2_channel_free((LIBSSH2_CHANNEL*)client->channel);
        
        while (!(client->channel = libssh2_channel_open_session((LIBSSH2_SESSION*)client->session))) {
            int last_error = libssh2_session_last_errno((LIBSSH2_SESSION*)client->session);
            if (last_error != LIBSSH2_ERROR_EAGAIN) {
                ssh_set_error(client, "Failed to reopen channel for bash");
                return false;
            }
        }
        
        // Try basic bash
        while ((rc = libssh2_channel_exec((LIBSSH2_CHANNEL*)client->channel, "/bin/bash")) == LIBSSH2_ERROR_EAGAIN);
        if (rc == 0) {
            printf("Connected with /bin/bash\n");
        } else {
            printf("bash failed (code: %d), trying sh...\n", rc);
            
            // Close and reopen channel for sh
            libssh2_channel_close((LIBSSH2_CHANNEL*)client->channel);
            libssh2_channel_free((LIBSSH2_CHANNEL*)client->channel);
            
            while (!(client->channel = libssh2_channel_open_session((LIBSSH2_SESSION*)client->session))) {
                int last_error = libssh2_session_last_errno((LIBSSH2_SESSION*)client->session);
                if (last_error != LIBSSH2_ERROR_EAGAIN) {
                    ssh_set_error(client, "Failed to reopen channel for sh");
                    return false;
                }
            }
            
            // Last resort: basic sh
            while ((rc = libssh2_channel_exec((LIBSSH2_CHANNEL*)client->channel, "/bin/sh")) == LIBSSH2_ERROR_EAGAIN);
            if (rc != 0) {
                char error_details[512];
                char* libssh2_error_msg;
                int libssh2_error_len;
                libssh2_session_last_error((LIBSSH2_SESSION*)client->session, &libssh2_error_msg, &libssh2_error_len, 0);
                
                snprintf(error_details, sizeof(error_details), "All shell startup attempts failed (code: %d, libssh2: %s)", 
                         rc, libssh2_error_msg ? libssh2_error_msg : "unknown");
                ssh_set_error(client, error_details);
                
                libssh2_channel_close((LIBSSH2_CHANNEL*)client->channel);
                libssh2_channel_free((LIBSSH2_CHANNEL*)client->channel);
                libssh2_session_disconnect((LIBSSH2_SESSION*)client->session, "Shell startup failed");
                libssh2_session_free((LIBSSH2_SESSION*)client->session);
                close(client->socket_fd);
                client->channel = NULL;
                client->session = NULL;
                client->socket_fd = -1;
                return false;
            }
            printf("Connected with /bin/sh\n");
        }
    }
    
    client->state = SSH_STATE_CONNECTED;
    return true;
}

bool ssh_client_send(ssh_client_t* client, const char* data, size_t len) {
    if (!client || !data || len == 0 || client->state != SSH_STATE_CONNECTED) {
        return false;
    }
    
    ssize_t bytes_written = 0;
    size_t total_written = 0;
    
    while (total_written < len) {
        bytes_written = libssh2_channel_write((LIBSSH2_CHANNEL*)client->channel, 
                                            data + total_written, 
                                            len - total_written);
        
        if (bytes_written == LIBSSH2_ERROR_EAGAIN) {
            continue; // Try again
        } else if (bytes_written < 0) {
            ssh_set_error(client, "Failed to send data");
            return false;
        }
        
        total_written += bytes_written;
    }
    
    return true;
}

int ssh_client_receive(ssh_client_t* client, char* buffer, size_t buffer_size) {
    if (!client || !buffer || buffer_size == 0 || client->state != SSH_STATE_CONNECTED) {
        return -1;
    }
    
    // Check if channel is closed by remote side
    if (libssh2_channel_eof((LIBSSH2_CHANNEL*)client->channel)) {
        snprintf(client->error_msg, sizeof(client->error_msg), 
                "Connection to %s closed.", client->hostname);
        client->state = SSH_STATE_DISCONNECTED;
        return -2; // Special return code for clean disconnect
    }
    
    ssize_t bytes_read = libssh2_channel_read((LIBSSH2_CHANNEL*)client->channel, buffer, buffer_size);
    
    if (bytes_read == LIBSSH2_ERROR_EAGAIN) {
        return 0; // No data available
    } else if (bytes_read < 0) {
        ssh_set_error(client, "Failed to receive data");
        return -1;
    } else if (bytes_read == 0) {
        // Zero bytes read might indicate EOF - double check
        if (libssh2_channel_eof((LIBSSH2_CHANNEL*)client->channel)) {
            snprintf(client->error_msg, sizeof(client->error_msg), 
                    "Connection to %s closed.", client->hostname);
            client->state = SSH_STATE_DISCONNECTED;
            return -2; // Special return code for clean disconnect
        }
    }
    
    return (int)bytes_read;
}

bool ssh_client_resize_pty(ssh_client_t* client, int width, int height) {
    if (!client || client->state != SSH_STATE_CONNECTED || !client->channel) {
        return false;
    }
    
    int rc;
    while ((rc = libssh2_channel_request_pty_size((LIBSSH2_CHANNEL*)client->channel, width, height)) == LIBSSH2_ERROR_EAGAIN);
    
    return rc == 0;
}

bool ssh_client_send_signal(ssh_client_t* client, const char* signal_name) {
    if (!client || !signal_name || client->state != SSH_STATE_CONNECTED || !client->channel) {
        return false;
    }
    
    int rc;
    while ((rc = libssh2_channel_signal((LIBSSH2_CHANNEL*)client->channel, signal_name)) == LIBSSH2_ERROR_EAGAIN);
    
    return rc == 0;
}

bool ssh_client_get_exit_status(ssh_client_t* client, int* exit_status) {
    if (!client || !exit_status || client->state != SSH_STATE_CONNECTED || !client->channel) {
        return false;
    }
    
    *exit_status = libssh2_channel_get_exit_status((LIBSSH2_CHANNEL*)client->channel);
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
    
    if (client->channel) {
        libssh2_channel_close((LIBSSH2_CHANNEL*)client->channel);
        libssh2_channel_free((LIBSSH2_CHANNEL*)client->channel);
        client->channel = NULL;
    }
    
    if (client->session) {
        libssh2_session_disconnect((LIBSSH2_SESSION*)client->session, "Client cleanup");
        libssh2_session_free((LIBSSH2_SESSION*)client->session);
        client->session = NULL;
    }
    
    if (client->socket_fd != -1) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    
    client->state = SSH_STATE_DISCONNECTED;
    memset(client->error_msg, 0, sizeof(client->error_msg));
}
