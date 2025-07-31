/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define AF_UNIX             1
#define PF_LOCAL            AF_UNIX
#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        10
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6
#define PF_UNSPEC       AF_UNSPEC

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_IPV6    41
#define IPPROTO_ICMPV6  58
#define IPPROTO_UDPLITE 136
#define IPPROTO_RAW     255

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define SO_REUSEADDR   0x0004 /* Allow local address reuse */
#define SO_KEEPALIVE   0x0008 /* keep connections alive */
#define SO_BROADCAST   0x0020 /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */

#define SO_DEBUG        0x0001 /* Unimplemented: turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002 /* socket has had listen() */
#define SO_DONTROUTE    0x0010 /* Unimplemented: just use interface addresses */
#define SO_USELOOPBACK  0x0040 /* Unimplemented: bypass hardware when possible */
#define SO_LINGER       0x0080 /* linger on close if data present */
#define SO_DONTLINGER   ((int)(~SO_LINGER))
#define SO_OOBINLINE    0x0100 /* Unimplemented: leave received OOB data in line */
#define SO_REUSEPORT    0x0200 /* Unimplemented: allow local address & port reuse */
#define SO_SNDBUF       0x1001 /* Unimplemented: send buffer size */
#define SO_RCVBUF       0x1002 /* receive buffer size */
#define SO_SNDLOWAT     0x1003 /* Unimplemented: send low-water mark */
#define SO_RCVLOWAT     0x1004 /* Unimplemented: receive low-water mark */
#define SO_SNDTIMEO     0x1005 /* send timeout */
#define SO_RCVTIMEO     0x1006 /* receive timeout */
#define SO_ERROR        0x1007 /* get error status and clear */
#define SO_TYPE         0x1008 /* get socket type */
#define SO_CONTIMEO     0x1009 /* Unimplemented: connect timeout */
#define SO_NO_CHECK     0x100a /* don't create UDP checksum */
#define SO_BINDTODEVICE 0x100b /* bind to device */

typedef uint32_t socklen_t;
typedef uint8_t sa_family_t;
typedef uint16_t in_port_t;

/* members are in network byte order */
struct sockaddr_in {
  uint8_t            sin_len;
  sa_family_t     sin_family;
  in_port_t       sin_port;
  struct in_addr  sin_addr;
  #define SIN_ZERO_LEN 8
  char            sin_zero[SIN_ZERO_LEN];
};

struct sockaddr_in6 {
  uint8_t            sin6_len;      /* length of this structure    */
  sa_family_t     sin6_family;   /* AF_INET6                    */
  in_port_t       sin6_port;     /* Transport layer port #      */
  uint32_t           sin6_flowinfo; /* IPv6 flow information       */
  struct in6_addr sin6_addr;     /* IPv6 address                */
  uint32_t           sin6_scope_id; /* Set of interfaces for scope */
};

struct sockaddr {
  uint8_t        sa_len;
  sa_family_t sa_family;
  char        sa_data[14];
};

struct sockaddr_storage {
  uint8_t        s2_len;
  sa_family_t ss_family;
  char        s2_data1[2];
  uint32_t       s2_data2[3];
  uint32_t       s2_data3[3];
};

int socketpair(int domain, int type, int protocol, int sv[2]);

int accept(int s,struct sockaddr *addr,socklen_t *addrlen);
int bind(int s,const struct sockaddr *name, socklen_t namelen);
int shutdown(int s,int how);
int getpeername(int s,struct sockaddr *name,socklen_t *namelen);
int getsockname(int s,struct sockaddr *name,socklen_t *namelen);
int setsockopt(int s,int level,int optname,const void *opval,socklen_t optlen);
int getsockopt(int s,int level,int optname,void *opval,socklen_t *optlen);
int closesocket(int s);
int connect(int s,const struct sockaddr *name,socklen_t namelen);
int listen(int s,int backlog);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
ssize_t recv(int s,void *mem,size_t len,int flags);
ssize_t recvfrom(int s,void *mem,size_t len,int flags,struct sockaddr *from,socklen_t *fromlen);
ssize_t send(int s,const void *dataptr,size_t size,int flags);
ssize_t sendmsg(int s,const struct msghdr *message,int flags);
ssize_t sendto(int s,const void *dataptr,size_t size,int flags,const struct sockaddr *to,socklen_t to_len);
int socket(int domain,int type,int protocol);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

#ifdef __cplusplus
}
#endif
