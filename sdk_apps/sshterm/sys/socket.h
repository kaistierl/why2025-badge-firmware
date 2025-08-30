/* sys/socket.h wrapper for badge environment */
#ifndef _SYS_SOCKET_WRAPPER_H
#define _SYS_SOCKET_WRAPPER_H

/* Include the needed address structures first */
#include <netinet/in.h>

/* Then include the original socket header */
#include_next <sys/socket.h>

#endif /* _SYS_SOCKET_WRAPPER_H */
