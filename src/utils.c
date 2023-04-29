#include "utils.h"
#include <stdio.h>
#include <fcntl.h>

int read_full(int fd, char *buf,size_t n) {
    while (n > 0) {
        size_t rv = read(fd, buf, n);
        if (rv <= 0) return -1;

        assert((size_t)rv <= n);
        n -= (size_t)rv;

        buf += rv;
    }

    return 0;
}

int write_full(int fd, unsigned char  *buf,size_t n) {
    while (n > 0) {
        size_t wv = write(fd, buf, n);
        if (wv <= 0) return -1;

        assert((size_t)wv <= n);
        n -= (size_t)wv;

        buf += wv;
    }

    return 0;
}

int cerr(int ret){

    if (ret < 0) {
        fprintf(stderr, "error: %d", errno);
        exit(1);
    }
    return ret;

}

int set_fd_nblocking(int fd) {
    int ret = fcntl(fd, F_SETFL, O_NONBLOCK); /*setting as nonblocking fd*/
    if (ret < 0) {
        fprintf(stderr, "Failed to set O_NONBLOCK");
        return -1;
    }
    return 0;
}
