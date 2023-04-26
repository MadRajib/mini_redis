#ifndef _UTILS_H
#define _UTILS_H
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define LEN_SIZE 4
#define K_MAX_MSG 4096

typedef struct{
    int len;
    char msg[K_MAX_MSG];
} pkt_t;

int read_full(int fd, char *buf,size_t n);
int write_full(int fd, char *buf,size_t n);
int cerr(int ret);
int set_fd_nblocking(int fd);

#endif
