#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "utils.h"
#include "list.h"

#define MAX_EVENTS 10
struct epoll_event ev, events[MAX_EVENTS];

enum {
    STATE_REQ=0, /*while reading*/
    STATE_RES=1, /*while writing*/
    STATE_END=2, /*to delete connection*/
};

typedef struct {
    int fd;
    uint32_t state;
    size_t rbuf_size;
    size_t wbuf_size;
    size_t wbuf_sent;
    uint8_t rbuf[LEN_SIZE + K_MAX_MSG];
    uint8_t wbuf[LEN_SIZE + K_MAX_MSG];
    struct list_head node;
} Conn_t;

struct list_head conn_list = LIST_INIT(conn_list);

static int process_request(int connfd){
    
    //4 bytes header
    char rbuf[LEN_SIZE + K_MAX_MSG + 1];
    errno = 0;
    pkt_t msg;
    
    ssize_t ret = read(connfd, rbuf, LEN_SIZE + K_MAX_MSG);
    if (ret <= 0) return -1;

    memcpy(&msg.len, rbuf, LEN_SIZE);

    if(msg.len > K_MAX_MSG) {
        printf("Too long mgs %d\n", msg.len);
        return -1;
    }

    memcpy(&msg.msg, rbuf + LEN_SIZE, msg.len);

    msg.msg[msg.len] = '\0';
    printf("client says: %s\n", msg.msg);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    int len = (int)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);

    return write_full(connfd, wbuf, 4 + len);

}

static int accept_new_conn(int connfd){
    
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int ret, fd;
    Conn_t *con;

    fd =  accept(connfd, (struct sockaddr *)&client_addr, &socklen);
    if (fd < 0) {
        fprintf(stderr,"error while accept %d\n", errno);
        return -1;
    }

    ret = set_fd_nblocking(fd); /*setting as nonblocking fd*/
    if (ret < 0) {
        fprintf(stderr,"error while not able to set non blocking %d\n", errno);
        close(fd);
        return -1;
    }

    con = (Conn_t *)malloc(sizeof(Conn_t));

    con->fd = fd;
    con->state = STATE_REQ;
    con->rbuf_size = 0;
    con->wbuf_size = 0;
    con->wbuf_sent = 0;

    list_add(&conn_list, &con->node);

    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int val = 1;
    int ret;
    struct sockaddr_in addr = {};
    int nfds, epollfd;


    int fd = cerr(socket(AF_INET, SOCK_STREAM, 0));
    cerr(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));

    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);

    cerr(bind(fd, (const struct sockaddr *)&addr, sizeof(addr)));    
    cerr(listen(fd, SOMAXCONN));


    ret = set_fd_nblocking(fd); /*setting as nonblocking fd*/
    if (ret < 0)
        goto ERROR;

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        fprintf(stderr, "epoll create1 failed %d\n",errno);
        goto ERROR;
    }

    ev.events = EPOLLIN;
    ev.data.fd = fd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        fprintf(stderr, "epoll_ctl: listen_sock %d\n", errno);
        goto ERROR;
    }
    
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);

    for(;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            fprintf(stderr, "epoll_wait %d\n", errno);
            goto ERROR;
        }

        for(int n=0; n < nfds; n++) {
            if(events[n].data.fd == fd) {
                int connfd =  accept(fd, (struct sockaddr *)&client_addr, &socklen);
                if (connfd < 0)
                    continue;
                printf("recived a connection using epoll\n");
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    fprintf(stderr, "epoll_ctl: %d\n", errno);
                    close(connfd);
                    goto ERROR;
                } 
            }else {
                ret = process_request(events[n].data.fd);
                if (ret) break;
            }
        }
/*
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd =  accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0)
            continue;
        
*/
        /*Handle multiple request*/
/*
        while (1) {
            ret = process_request(connfd);
            if (ret) break;
        }
        close(connfd);
*/
    

    }
ERROR:
    close(fd);
    return 0;
}
