/* sys/socket.h wrapper for badge environment
 *
 * The BadgeVMS SDK's sys/socket.h (picolibc/newlib) does not pull in
 * <netinet/in.h> on its own, which means struct sockaddr_in and the AF_*
 * constants are undefined when socket.h is included first. wolfSSH and
 * wolfSSL both hit this. This wrapper forces the correct include order and
 * then chains to the real header via #include_next.
 */
#ifndef _SYS_SOCKET_WRAPPER_H
#define _SYS_SOCKET_WRAPPER_H

#include <netinet/in.h>
#include_next <sys/socket.h>

#endif /* _SYS_SOCKET_WRAPPER_H */
