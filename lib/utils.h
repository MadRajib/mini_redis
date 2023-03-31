#ifndef _UTILS_H
#define _UTILS_H
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define K_MAX_MSG 4096

int read_full(int fd, char *buf,size_t n);
int write_full(int fd, char *buf,size_t n);

#endif
