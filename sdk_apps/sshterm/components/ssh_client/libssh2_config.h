/* libssh2_config.h - Configuration for libssh2 on ESP32P4 with mbedTLS */

#ifndef LIBSSH2_CONFIG_H
#define LIBSSH2_CONFIG_H

/* Basic system headers */
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_UNISTD_H 1

/* Basic functions */
#define HAVE_GETTIMEOFDAY 1
#define HAVE_MEMSET_S 1
#define HAVE_SNPRINTF 1
#define HAVE_STRCASECMP 1
#define HAVE_STRTOLL 1

/* Network-related functions */
#define HAVE_RECV 1
#define HAVE_SEND 1
#define HAVE_SELECT 1
#define HAVE_SOCKET 1

/* Crypto backend - using mbedTLS */
#define LIBSSH2_MBEDTLS 1

/* Disable features we don't need to save space */
#define LIBSSH2_NO_SFTP 1
#define LIBSSH2_NO_SCP 1

/* Memory management */
#define LIBSSH2_NO_CLEAR_MEMORY 1

/* Threading - disable for simplicity */
/* #undef LIBSSH2_PTHREAD */

/* Compression - disable to save space */
/* #undef LIBSSH2_HAVE_ZLIB */

/* API visibility */
#define LIBSSH2_API

/* Platform specific */
#define LIBSSH2_DH_GEX_NEW 1

#endif /* LIBSSH2_CONFIG_H */