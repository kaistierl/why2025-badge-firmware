/* custom_io.c - I/O callbacks for wolfSSH on BadgeVMS
 *
 * Two recv callbacks, identical blocking behaviour on both local and hardware builds:
 *
 *   wolfssh_io_recv          — used during wolfSSH_connect (handshake + auth).
 *                              wolfSSH's handshake state machine cannot be re-entered
 *                              iteratively across the auth phase; recv must block.
 *
 *   wolfssh_io_recv_streaming — installed after the handshake succeeds. Currently
 *                              identical to wolfssh_io_recv (blocking read). Kept
 *                              separate so a non-blocking variant can be introduced
 *                              later if BadgeVMS exposes SO_RCVTIMEO or MSG_DONTWAIT.
 *
 * Both local and hardware builds use the same two-thread model: ssh_io_thread for
 * recv and read_input_thread for sends, so local test results reflect hardware
 * behaviour faithfully.
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <wolfssh/ssh.h>
#include <wolfssh/error.h>

/* Blocking recv — used during wolfSSH_connect (handshake + auth). */
int wolfssh_io_recv(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    (void)ssh;
    int sock_fd = *(int*)ctx;

    if (sock_fd < 0) {
        printf("SSH_IO: Invalid socket fd: %d\n", sock_fd);
        return WS_SOCKET_ERROR_E;
    }

    int bytes_read = (int)read(sock_fd, data, size);
    if (bytes_read == -1) {
        printf("SSH_IO: Read error: %s (errno=%d)\n", strerror(errno), errno);
        return WS_SOCKET_ERROR_E;
    } else if (bytes_read == 0) {
        return WS_CBIO_ERR_CONN_CLOSE;
    }
    return bytes_read;
}

/* Streaming recv — installed after the handshake succeeds.
 * Currently identical to wolfssh_io_recv. */
int wolfssh_io_recv_streaming(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    return wolfssh_io_recv(ssh, data, size, ctx);
}

/* Blocking send callback. */
int wolfssh_io_send(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    (void)ssh;
    int sock_fd = *(int*)ctx;

    if (sock_fd < 0) {
        printf("SSH_IO: Invalid socket fd: %d\n", sock_fd);
        return WS_SOCKET_ERROR_E;
    }

    int bytes_written = (int)write(sock_fd, data, size);
    if (bytes_written == -1) {
        printf("SSH_IO: Write error: %s (errno=%d)\n", strerror(errno), errno);
        return WS_SOCKET_ERROR_E;
    }
    return bytes_written;
}

#ifndef SSHTERM_LOCAL_BUILD
/* setsockopt is not in the BadgeVMS shared symbol table. Provide a local no-op
 * so the ELF resolves the symbol from its own .text. */
int setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}
#endif

/* Install the blocking recv callback on the wolfSSH context (handshake phase).
 * ssh_client.c switches to wolfssh_io_recv_streaming after the handshake. */
void setup_wolfssh_custom_io(WOLFSSH_CTX* ctx) {
    if (ctx == NULL) {
        printf("SSH: ERROR - ctx is NULL!\n");
        return;
    }
    wolfSSH_SetIORecv(ctx, wolfssh_io_recv);
    wolfSSH_SetIOSend(ctx, wolfssh_io_send);
}
