/* custom_io.c - Blocking I/O callbacks for wolfSSH on BadgeVMS
 * Uses read/write directly with blocking behavior in separate thread
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <wolfssh/ssh.h>
#include <wolfssh/error.h>

/* Custom recv callback for wolfSSH - BLOCKING VERSION */
int wolfssh_io_recv(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    (void)ssh; // Unused parameter
    int sock_fd = *(int*)ctx;
    
    // Basic socket validation
    if (sock_fd < 0) {
        printf("SSH_IO: Invalid socket fd: %d\n", sock_fd);
        return WS_SOCKET_ERROR_E;
    }
    
    // Blocking read - we handle timeouts at higher level
    int bytes_read = read(sock_fd, data, size);
    
    if (bytes_read == -1) {
        printf("SSH_IO: Read error: %s (errno=%d)\n", strerror(errno), errno);
        return WS_SOCKET_ERROR_E;
    } else if (bytes_read == 0) {
        // TCP connection closed cleanly by remote end
        return WS_CBIO_ERR_CONN_CLOSE;
    }
    
    // Successfully read data
    return bytes_read;
}

/* Custom send callback for wolfSSH - BLOCKING VERSION */
int wolfssh_io_send(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    (void)ssh; // Unused parameter
    int sock_fd = *(int*)ctx;
    
    // Basic socket validation
    if (sock_fd < 0) {
        printf("SSH_IO: Invalid socket fd: %d\n", sock_fd);
        return WS_SOCKET_ERROR_E;
    }
    
    // In blocking mode, just call write() directly - it will block until data is sent
    int bytes_written = write(sock_fd, data, size);
    
    if (bytes_written == -1) {
        printf("SSH_IO: Write error: %s (errno=%d)\n", strerror(errno), errno);
        return WS_SOCKET_ERROR_E;
    }
    
    // Successfully wrote data
    return bytes_written;
}

#ifndef SSHTERM_LOCAL_BUILD
/* setsockopt is not in the BadgeVMS shared symbol table — the ELF loader fails
 * with "Can't find common setsockopt". Provide a local no-op so the symbol is
 * resolved from the ELF's own .text rather than from the shared table.
 * Consequence: SO_RCVTIMEO cannot be configured; wolfSSH_stream_read therefore
 * continues to block indefinitely, and wolfssh_mutex must NOT be held across
 * it (would deadlock read_input_thread). See ssh_client_receive in ssh_client.c. */
int setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}
#endif

/* Setup wolfSSH custom I/O */
void setup_wolfssh_custom_io(WOLFSSH_CTX* ctx) {
    if (ctx == NULL) {
        printf("SSH: ERROR - ctx is NULL!\n");
        return;
    }
    
    // Setup I/O callbacks
    wolfSSH_SetIORecv(ctx, wolfssh_io_recv);
    wolfSSH_SetIOSend(ctx, wolfssh_io_send);
}
