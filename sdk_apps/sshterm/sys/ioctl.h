/* sys/ioctl.h wrapper for badge environment
 *
 * wolfSSH's io.c includes <sys/ioctl.h> for the FIONBIO symbol at compile
 * time. This header satisfies that include on BadgeVMS where no system
 * ioctl.h exists.
 *
 * At runtime the stub is never called: WOLFSSH_USER_IO (set in
 * user_settings.h) makes wolfSSH use the custom I/O callbacks in
 * custom_io.c instead of its default socket I/O, so the code path that
 * would call ioctl(fd, FIONBIO, ...) to set non-blocking mode is never
 * compiled into the active build. The stub exists purely to prevent a
 * missing-symbol link error if that assumption ever changes.
 */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#define FIONBIO 0x5421

static inline int ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    if (request == FIONBIO) {
        return 0;
    }
    return -1;
}

#endif /* _SYS_IOCTL_H */
