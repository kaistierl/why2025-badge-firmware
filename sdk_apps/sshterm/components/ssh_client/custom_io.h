/* custom_io.h - Simplified blocking I/O callbacks for wolfSSH on BadgeVMS */

#ifndef CUSTOM_IO_H
#define CUSTOM_IO_H

/* wolfSSH custom I/O callbacks */
#include <wolfssh/ssh.h>

int wolfssh_io_recv(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);
int wolfssh_io_recv_streaming(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);
int wolfssh_io_send(WOLFSSH* ssh, void* buf, word32 sz, void* ctx);
void setup_wolfssh_custom_io(WOLFSSH_CTX* ctx);

#endif /* CUSTOM_IO_H */
