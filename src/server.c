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

    CMD_SET=0,
    CMD_DEL=1,
    CMD_GET=3,
};

typedef struct {
    int fd;
    uint32_t state;
    size_t rbuf_size;
    size_t wbuf_size;
    size_t wbuf_sent;
    uint8_t rbuf[LEN_SIZE + K_MAX_MSG + 1];
    uint8_t wbuf[LEN_SIZE + K_MAX_MSG + 1];
    struct list_head node;
} Conn_t;

typedef struct {
    uint32_t id;
    uint8_t key[80];
    uint8_t val[80];
    struct list_head node;
} db_item_t;

struct list_head conn_list = LIST_INIT(conn_list);
struct list_head in_mem_db = LIST_INIT(in_mem_db);

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

    return fd;
}

int c_io_err(int code) {
    if (code <= 0)
        fprintf(stderr, "reading 0 or less bytes erro: %d\n", errno);
    return code;
}

#define MAX_CMD_IN_REQUEST 5

/* code eg set del get , char key, val char*/
typedef struct{
    uint32_t cmd_code;
    uint32_t key;
    uint32_t value;
}Cmd_t;

uint32_t get_cmd_code(char *code) {
    if(strcmp(code, "GET")) 
        return CMD_GET;
    else if(strcmp(code, "DEL"))
        return CMD_DEL;
    else if(strcmp(code, "SET"))
        return CMD_SET;
    else 
        return -1;
}

void print_cmd(Cmd_t *cmd){
    printf("cmd : code:%d\n", cmd->cmd_code);
    printf("cmd : key:%d\n", cmd->key);
    printf("cmd : val:%d\n", cmd->value);
}

Cmd_t *parse_cmd(char *cmd_str, size_t len) {
    char *cmd_ptr = NULL;
    char *saveptr = NULL;
    char *delim = " ";
    Cmd_t *cmd = malloc(sizeof(Cmd_t));
    int count = 0;
    memset(cmd, 0, sizeof(Cmd_t));

    cmd_ptr = strtok_r(cmd_str, delim, &saveptr);
    while (cmd_ptr!= NULL) {
        printf("%s\n", cmd_ptr);
        switch(count) {
            case 0:
                cmd->cmd_code = get_cmd_code(cmd_ptr);
                break;
            case 1:
                cmd->key = atoll(cmd_ptr);
                break;
            case 2:
                cmd->value = atoll(cmd_ptr);
                break;
            default:
                goto ERROR;
        }
        cmd_ptr = strtok_r(NULL, delim, &saveptr);
        count++;
    }
    print_cmd(cmd);
    return cmd;
ERROR:
    free(cmd);
    cmd = NULL;
    return NULL;
}

void add_to_db(uint32_t key, uint32_t val) {

}

void get_from_db(uint32_t key) {
    
}

void del_from_db(uint32_t key) {

}

void mod_in_db(uint32_t key, uint32_t val) {

}

void process_cmd(Cmd_t *cmd) {

   switch (cmd->cmd_code) {
        case CMD_GET:
            get_from_db(cmd->key);
            break;
        case CMD_SET:
            add_to_db(cmd->key, cmd->value);
            break;
        case CMD_DEL:
            del_from_db(cmd->key);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid cmd code %d", cmd->cmd_code);
    } 
}

void process_raw_data(Conn_t *conn) {
    int ret;
    uint8_t nstrs = 0; /* to store number of strings*/
    uint8_t len = 0;  /* to store len of each string*/
    uint8_t *ptr = NULL;
    char data[80];
    Cmd_t *cmd;

    /*
     * nstr : len1 : str1: len2: str2:.... :lenn: strn
     */
    ret = c_io_err(read(conn->fd, conn->rbuf, K_MAX_MSG));
    if (ret <= 0) {
        conn->state = STATE_END;
        return;
    }

    ptr = conn->rbuf;
    nstrs = *(uint8_t *)ptr;
    assert(nstrs <= MAX_CMD_IN_REQUEST);
    ptr += sizeof(uint8_t);

    printf("Number of cmds : %d\n", nstrs);

    for (int n = 0; n < nstrs; n++ ){
        len = *(uint8_t *)ptr;
        assert(len > 0);
        ptr += sizeof(uint8_t);
        memcpy(&data, ptr, len);
        data[len] = '\0';
        ptr += len;

        printf("    sr: %d cmd :%s\n", n, data);
        cmd = parse_cmd(data, len);
        if (cmd != NULL) {
            process_cmd(cmd);
        }
    }

    memset(conn->rbuf, 0, K_MAX_MSG);
}

void process_request_state(Conn_t *conn) { 
    pkt_t req, resp;

    ssize_t ret = read(conn->fd, conn->rbuf, LEN_SIZE + K_MAX_MSG + 1);
    if (ret <= 0) {
        fprintf(stderr, "reading 0 bytes\n");
        conn->state = STATE_END;
        return;
    }

    memcpy(&req.len, conn->rbuf, LEN_SIZE);

    if(req.len > K_MAX_MSG) {
        printf("Too long mgs %d\n", req.len);
        return;
    }

    memcpy(&req.msg, conn->rbuf + LEN_SIZE, req.len);

    req.msg[req.len] = '\0';
    printf("client says: %s\n", req.msg);


    const char reply[] = "world";
    int len = (int)strlen(reply);
    memcpy(conn->wbuf, &len, 4);
    memcpy(conn->wbuf + 4, reply, len);
    conn->wbuf_size = 4 + len;

    conn->state = STATE_RES;

    return;

}

void process_response_state(Conn_t *conn) { 
    int ret;
    ret = write_full(conn->fd, conn->wbuf, conn->wbuf_size);
    if (ret < 0) {
        printf("error writing to the fd %d errno:%d\n", conn->fd, errno);
        //conn->state = STATE_END;
    }
     conn->state = STATE_REQ;

    return;
}

void process_connection_io(Conn_t *conn){
    switch (conn->state ){
        case STATE_REQ:
            process_raw_data(conn);
            //process_request_state(conn);
            break;
        case STATE_RES:
            process_response_state(conn);
            break;
        default:
            assert(0);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int val = 1;
    int ret;
    struct sockaddr_in addr = {};
    int nfds, epollfd;
    struct list_head *next;
    struct list_head *item;
    Conn_t *conn;


    int soc_fd = cerr(socket(AF_INET, SOCK_STREAM, 0));
    cerr(setsockopt(soc_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)));

    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);

    cerr(bind(soc_fd, (const struct sockaddr *)&addr, sizeof(addr)));    
    cerr(listen(soc_fd, SOMAXCONN));


    ret = set_fd_nblocking(soc_fd); /*setting as nonblocking fd*/
    if (ret < 0)
        goto ERROR;

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        fprintf(stderr, "epoll create1 failed %d\n",errno);
        goto ERROR;
    }

    ev.events = EPOLLIN;
    ev.data.fd = soc_fd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, soc_fd, &ev) == -1) {
        fprintf(stderr, "epoll_ctl: listen_sock %d\n", errno);
        goto ERROR;
    }
    
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);

    for(;;) {
        
        /*
         * Change epoll event type based on reading or writing to !
         */
        item = NULL;
        next = NULL;
        conn = NULL;
        list_for_each_safe(item, next, &conn_list) {
            conn = container_of(item, Conn_t, node);
            if (conn && (conn->fd > -1)) {
                ev.events = (conn->state == STATE_REQ) ? EPOLLIN : EPOLLOUT | EPOLLET ;
                ev.data.fd = conn->fd;
                if (epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->fd, &ev) == -1) {
                    fprintf(stderr, "epoll_ctl:  EPOLL_CTL_MOD failed for fd:%d errno %d\n", conn->fd, errno);
                }
            }
        }

        /*
         * wait for events with the instance fd
         */
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            fprintf(stderr, "epoll_wait %d\n", errno);
            goto ERROR;
        }

        for(int n=0; n < nfds; n++) {
            /*
            * Listen for new socket connection and register a new epoll event
            * for the corresponding socket.
            */
            if(events[n].data.fd == soc_fd) {
                int connfd =  accept_new_conn(soc_fd);
                if (connfd < 0)
                    continue;
                printf("recived a connection using epoll\n");
                ev.events = EPOLLIN | EPOLLET | EPOLLERR;
                ev.data.fd = connfd;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    fprintf(stderr, "epoll_ctl: %d\n", errno);
                    close(connfd);
                    goto ERROR;
                }
    
            }else{
                item = NULL;
                next = NULL;
                conn = NULL;
                list_for_each_safe(item, next, &conn_list) {

                    conn = container_of(item, Conn_t, node);
                    if (conn->fd == events[n].data.fd) {
                        //TODO
                        
                        printf("recived event : %d on fd %d\n", events[n].events , conn->fd);
                        process_connection_io(conn);

                        if ( conn->state == STATE_END ) {
                            printf("removing fd from the list\n");
                            list_del(&conn->node);
                            close(conn->fd);
                            free(conn);
                            conn = NULL;
                        } 
                    }
                }
                
            }
        }  

    }
ERROR:
    close(soc_fd);
    return 0;
}
