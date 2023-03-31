#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define K_MAX_MSG 4096

int cerr(int ret){

    if (ret < 0) {
        fprintf(stderr, "error: %d", errno);
        exit(1);
    }
    return ret;

}

static int read_full(int fd, char *buf,size_t n) {
    while (n > 0) {
        size_t rv = read(fd, buf, n);
        if (rv <= 0) return -1;

        assert((size_t)rv <= n);
        n -= (size_t)rv;

        buf += rv;
    }

    return 0;
}

static int write_full(int fd, char *buf,size_t n) {
    while (n > 0) {
        size_t wv = write(fd, buf, n);
        if (wv <= 0) return -1;

        assert((size_t)wv <= n);
        n -= (size_t)wv;

        buf += wv;
    }

    return 0;
}

static int process_request(int connfd){
    
    //4 bytes header
    char rbuf[4 + K_MAX_MSG + 1];
    errno = 0;
    
    ssize_t ret = read_full(connfd, rbuf, 4);
    if(ret) {
        if(ret == 0)
            printf("EOF \n");
        else
            printf("read() error");

        return ret;
    }

    int len = 0;
    memcpy(&len, rbuf, 4);

    if(len > K_MAX_MSG) {
        printf("Too long mgs\n");
        return -1;
    }

    ret = read_full(connfd, &rbuf[4], len);
    if(ret) {
        printf("error() error");
        return ret;
    }

    rbuf[4 + len] = '\0';
    printf("client says: %s\n", &rbuf[4]);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (int)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);

    return write_full(connfd, wbuf, 4 + len);

}
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int val = 1;
    int ret;
    struct sockaddr_in addr = {};


    int fd = cerr(socket(AF_INET, SOCK_STREAM, 0));
    cerr(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));

    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);

    cerr(bind(fd, (const struct sockaddr *)&addr, sizeof(addr)));
    
    cerr(listen(fd, SOMAXCONN));

    while(1) {

        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd =  accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0)
            continue;

        /*Handle multiple request*/
        while (1) {
            ret = process_request(connfd);
            if (ret) break;
        }
        close(connfd);

    }

    return 0;
}
