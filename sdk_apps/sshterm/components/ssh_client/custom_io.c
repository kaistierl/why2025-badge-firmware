/* custom_io.c - I/O callbacks for wolfSSH on BadgeVMS
 *
 * Two recv callbacks:
 *
 *   wolfssh_io_recv          — blocking read(), used during wolfSSH_connect.
 *                              wolfSSH's handshake state machine cannot be re-entered
 *                              iteratively across the auth phase; recv must block.
 *                              ctx = &client->socket_fd
 *
 *   wolfssh_io_recv_streaming — non-blocking drain from cipher_rb, installed after
 *                              the handshake. Returns WS_CBIO_ERR_WANT_READ when the
 *                              ring buffer is empty. sock_reader_thread fills it.
 *                              ctx = ssh_client_t* (switched in ssh_run_handshake)
 */

#include "ssh_client.h"  /* ssh_client_cipher_rb_drain, ssh_client_t */

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

/* Streaming recv — installed after the handshake. Drains cipher_rb non-blockingly.
 * ctx is ssh_client_t* (set in ssh_run_handshake via wolfSSH_SetIOReadCtx).
 * Returns WS_CBIO_ERR_WANT_READ when buffer is empty (sock_reader_thread still alive),
 * or WS_CBIO_ERR_CONN_CLOSE when the socket has closed, causing wolfSSH_stream_read
 * to propagate WS_EOF → ssh_client_receive returns -2 → io_thread posts disconnect. */
int wolfssh_io_recv_streaming(WOLFSSH* ssh, void* data, word32 size, void* ctx) {
    (void)ssh;
    ssh_client_t* client = (ssh_client_t*)ctx;
    int n = ssh_client_cipher_rb_drain(client, data, (int)size);
    if (n > 0) return n;
    return client->socket_closed ? WS_CBIO_ERR_CONN_CLOSE : WS_CBIO_ERR_WANT_READ;
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
#include <sys/socket.h>  /* socklen_t, ssize_t — hw only */

/* Symbol stubs for BadgeVMS ELF compatibility.
 *
 * These symbols are not in the BadgeVMS shared symbol table (confirmed by ELF
 * load failures: "Can't find common <symbol>"). Local definitions satisfy the
 * dynamic linker so the ELF loads from its own .text section.
 *
 * setsockopt  — not available; no-op (we never need SO_RCVTIMEO on this path).
 * recv        — not available; returns ENOSYS. All socket reads use read().
 * fcntl       — not available; returns ENOSYS. O_NONBLOCK therefore unavailable.
 *               Consequence: full Option A (cipher ring buffer + sock_reader_thread)
 *               is required — see WOLFSSH_THREADING_PLAN_INPUT.md.
 */
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

ssize_t recv(int s, void* mem, size_t len, int flags) {
    (void)s; (void)mem; (void)len; (void)flags;
    errno = ENOSYS;
    return -1;
}

int fcntl(int fd, int cmd, ...) {
    (void)fd; (void)cmd;
    errno = ENOSYS;
    return -1;
}
#endif /* SSHTERM_LOCAL_BUILD */

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
