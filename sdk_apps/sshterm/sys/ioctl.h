/* sys/ioctl.h wrapper for badge environment */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/* Badge environment doesn't support ioctl, but wolfSSH expects these */
#define FIONBIO 0x5421

/* Minimal ioctl function stub */
static inline int ioctl(int fd, unsigned long request, ...) {
    /* Always return success for FIONBIO (non-blocking IO), fail for others */
    if (request == FIONBIO) {
        return 0;
    }
    return -1;
}

#endif /* _SYS_IOCTL_H */
