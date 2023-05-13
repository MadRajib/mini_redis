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
#include "db.h"
#include "utils.h"
#include "list.h"

#define MAX_EVENTS 10
#define MAX_CMD_IN_REQUEST 5

enum {
    STATE_REQ=0, /*while reading*/
    STATE_RES=1, /*while writing*/
    STATE_END=2, /*to delete connection*/

    CMD_SET=0,
    CMD_DEL=1,
    CMD_GET=3,
    CMD_MOD=4,

    INAVLID_READ        =0,
    INAVLID_NSTR        =1,
    INAVLID_CMD_LEN     =2,
    INAVLID_CMD_BYTE    =3,

    INVALID_CMD=1,
    INVALID_CMD_CODE=2,
    INVALID_CMD_KEY=3,
    INVALID_CMD_VALUE=4,
};

struct epoll_event ev, events[MAX_EVENTS];

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

/* code eg set del get , char key, val char*/
typedef struct{
    uint32_t cmd_code;
    uint32_t key;
    uint32_t value;
    int ret;
}Cmd_t;

struct list_head conn_list = LIST_INIT(conn_list);

int c_io_err(int code) {
    printf("%s\n",__func__);
    if (code <= 0)
        fprintf(stderr, "reading 0 or less bytes erro: %d\n", errno);
    return code;
}

static int accept_new_conn(int connfd){
    printf("%s\n",__func__);
    
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

uint32_t get_cmd_code(char *code) {
    printf("%s\n",__func__);
    if(strcmp(code, "GET") == 0) 
        return CMD_GET;
    else if(strcmp(code, "DEL") == 0)
        return CMD_DEL;
    else if(strcmp(code, "SET") == 0)
        return CMD_SET;
    else if(strcmp(code, "MOD") == 0)
        return CMD_MOD;
    else 
        return -1;
}

void parse_cmd(char *cmd_str, size_t len, Cmd_t *cmd) {
    printf("%s\n",__func__);
    char *cmd_ptr = NULL;
    char *saveptr = NULL;
    char *delim = " ";
    int count = 0;
    int ret = 0;
    
    char *cmd_parts[3] = {NULL, NULL, NULL};

    cmd_ptr = strtok_r(cmd_str, delim, &saveptr);
    while (cmd_ptr!= NULL) {
        count++;
        if(count > 3) {
            cmd->ret = -INVALID_CMD;
            return;
        } 
        cmd_parts[count -1] = cmd_ptr;
        cmd_ptr = strtok_r(NULL, delim, &saveptr);
    }

    if(count < 1 || !cmd_parts[0]){
        cmd->ret = -INVALID_CMD;
        return;
    }

    ret = get_cmd_code(cmd_parts[0]);
    if (ret < 0) { 
        cmd->ret = -INVALID_CMD_CODE;
        return;
    }
    cmd->cmd_code = ret;

    if(count < 2 || !cmd_parts[1]) {
        cmd->ret = -INVALID_CMD;
        return; 
    }

    ret = atoll(cmd_parts[1]);
    if (ret == 0) { 
        cmd->ret = -INVALID_CMD_KEY;
        return;
    }
    cmd->key = ret;

    if( cmd->cmd_code == CMD_SET || cmd->cmd_code == CMD_MOD ) {
        if(count < 3 || !cmd_parts[2]) {
            cmd->ret = -INVALID_CMD;
            return;
        }
        
        ret = atoll(cmd_parts[2]);
        if (ret == 0) {
            cmd->ret = -INVALID_CMD_VALUE;
            return;
        }
        cmd->value = ret;
    } else if(count > 2 || count > 3){ 
        cmd->ret = -INVALID_CMD;
        return;
    }

}

void process_cmd(Cmd_t *cmd, int fd) {
    printf("%s\n",__func__);
    if (cmd->ret < 0)
        return;

    Result_t ret = {-1};
     
    switch (cmd->cmd_code) {
        case CMD_GET:
            ret = get_from_db(cmd->key, fd);
            cmd->value = ret.val;
            break;
        case CMD_SET: 
            ret = add_to_db(cmd->key, cmd->value, fd);
            break;
        case CMD_DEL:
            ret = del_from_db(cmd->key, fd);
            break;
        case CMD_MOD:
            ret = mod_in_db(cmd->key, cmd->value, fd);
            break;
        default:
            ret.ret = -1;
            fprintf(stderr, "ERROR: Invalid cmd code %d", cmd->cmd_code);
    }

    cmd->ret = ret.ret;
}

int process_raw_data(Conn_t *conn) {
    printf("%s\n",__func__);
    int ret;
    uint32_t read_len;
    uint8_t nstrs = 0; /* to store number of strings*/
    uint8_t len = 0;  /* to store len of each string*/
    uint8_t *ptr = NULL;
    uint8_t *wptr = NULL;
    char data[80];
    Cmd_t *cmd;
    Result_t result;
    result.ret = -1;
    /*
     * nstr : len1 : str1: len2: str2:.... :lenn: strn
     */
    read_len = 0;
    ret = c_io_err(read(conn->fd, conn->rbuf, K_MAX_MSG));
    if (ret <= 0) {
        ret = -INAVLID_READ;
        goto ERROR;
    }
    conn->rbuf_size = ret;

    printf("read data %d\n", read_len);

    ptr = conn->rbuf;
    
    read_len +=sizeof(uint8_t);
    if (read_len > conn->rbuf_size) {
        ret = -INAVLID_NSTR;
        goto ERROR;
    }

    nstrs = *(uint8_t *)ptr;
    assert(nstrs <= MAX_CMD_IN_REQUEST);
    
    ptr += sizeof(uint8_t);

    //printf("Number of cmds : %d\n", nstrs);
    wptr = conn->wbuf;
    conn->wbuf_size = 0;
    memset(conn->wbuf, 0, K_MAX_MSG);
    wptr += sizeof(uint8_t);
    conn->wbuf_size += sizeof(uint8_t);

    for (int n = 0; n < nstrs; n++ ){
        
        read_len +=sizeof(uint8_t); 
        if (read_len > conn->rbuf_size) {
            ret = -INAVLID_CMD_LEN;
            goto ERROR;
        }

        len = *(uint8_t *)ptr;
        if (len < 0) { 
            ret = -INAVLID_CMD_LEN;
            goto ERROR;
        }

        ptr += sizeof(uint8_t);
    
        read_len += len; 
        if (read_len > conn->rbuf_size) {
            ret = -INAVLID_CMD_BYTE;
            goto ERROR;
        }

        memcpy(&data, ptr, len);
        data[len] = '\0';
        ptr += len;
        
        ret = -1;
        cmd = (Cmd_t *)malloc(sizeof(Cmd_t));
        memset(cmd, 0, sizeof(Cmd_t));

        parse_cmd(data, len, cmd); 

        process_cmd(cmd, conn->fd);
        
        (*(uint8_t *)conn->wbuf)++;
            
        memcpy(wptr, &cmd->ret, sizeof(int));
        wptr += sizeof(int);
        conn->wbuf_size += sizeof(int);

        memcpy(wptr, &cmd->value, sizeof(uint32_t));
        wptr += sizeof(uint32_t);
        conn->wbuf_size += sizeof(uint32_t);

        free(cmd);
        cmd = NULL;
    }

ERROR:
    memset(conn->rbuf, 0, K_MAX_MSG);
    conn->rbuf_size = 0;
    return ret;
}

int process_response_state(Conn_t *conn) { 
    printf("%s\n",__func__);
    int ret;
    ret = write_full(conn->fd, conn->wbuf, conn->wbuf_size);
    if (ret < 0) {
        printf("error writing to the fd %d errno:%d\n", conn->fd, errno);
        return -1;
    }
    return 0;
}

void process_connection_io(Conn_t *conn){
    printf("%s\n",__func__);
    int ret = 0;
    switch (conn->state ){
        case STATE_REQ:
            ret = process_raw_data(conn);
            //process_request_state(conn);
            if (ret == INAVLID_READ)
                conn->state = STATE_END;
            else
                conn->state = STATE_RES;

            break;
        case STATE_RES:
            process_response_state(conn);
            if (ret < 0)
                conn->state = STATE_END;
            else
                conn->state = STATE_REQ;
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
                ev.events = ((conn->state == STATE_REQ) ? EPOLLIN : EPOLLOUT);
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
                //printf("recived a connection using epoll\n");
                ev.events = EPOLLIN | EPOLLET;
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
                        
                        //printf("recived event : %d on fd %d\n", events[n].events , conn->fd);
                        process_connection_io(conn);

                        if ( conn->state == STATE_END ) {
                            printf("removing fd from the list\n");
                            remove_all_recod(conn->fd);
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
