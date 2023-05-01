#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "utils.h"

static void process_connection(int connfd){
    
    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));

    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) -1);
    if (n < 0) {
        printf("read() error : %d\n", errno);
        return;
    }
    printf("server says: %s\n", rbuf);

}

int add_cmd(char *buf, char *cmd) {
    
    uint8_t len = 0;
    uint8_t cm_len = (uint8_t)strlen(cmd);

    memcpy(buf, &cm_len, sizeof(uint8_t));
    len += sizeof(uint8_t);
    buf += sizeof(uint8_t);

    memcpy(buf, cmd, cm_len);
    len += cm_len;
    buf += cm_len;

    return len;
}

static int send_cmds(int fd, char *text) {
    char wbuf[K_MAX_MSG];
    char *ptr = NULL;
    char *cmd_ptr = NULL;
    char *saveptr = NULL;
    const char delim[] = ";";
    uint16_t pkt_len;
    int ret;
    uint8_t nstrs = 0;

    ptr = wbuf;
    /* jump to string starting poiunt*/
    ptr += sizeof(uint8_t);
    pkt_len += sizeof(uint8_t);

    cmd_ptr = strtok_r(text, delim, &saveptr);
    while (cmd_ptr != NULL) {
        nstrs++; 
        ret = add_cmd(ptr, cmd_ptr); 
        ptr += ret;
        pkt_len += ret;
        cmd_ptr = strtok_r(NULL, delim, &saveptr);
    }
    
    /* copy nstrings to the pkt buffer */
    memcpy(wbuf, &nstrs, sizeof(uint8_t));

    ret = write_full(fd, wbuf, pkt_len);
    if(ret)
        return ret;

    return 0;
}

static int query(int fd, const char *text) {
    int len = (int) strlen(text);
    if (len > K_MAX_MSG) return -1;

    char wbuf[4 + K_MAX_MSG];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    int ret = write_full(fd, wbuf, 4 + len);

    if(ret) {
        return ret;
    }

    char rbuf[4 + K_MAX_MSG +1];
    errno = 0;
    ret = read_full(fd, rbuf, 4);
    if(ret) {
        if(errno == 0)
            printf("EOF\n");
        else
            printf("read() error\n");

        return ret;
    }

    memcpy(&len, rbuf, 4);
    if(len > K_MAX_MSG) {
        printf("Too Long\n");
        return -1;
    }

    ret = read_full(fd, &rbuf[4], len);
    if (ret) {
        printf("read() error");
        return ret;
    }

    rbuf[4 + len] = '\0';
    printf("server says:%s\n", &rbuf[4]);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int val = 1;
    int ret;
    struct sockaddr_in addr = {};


    int fd = cerr(socket(AF_INET, SOCK_STREAM, 0));

    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    cerr(connect(fd, (const struct sockaddr *)&addr, sizeof(addr)));
    
    // mutlitple requests
    char test[80];
    strcpy(test, "SET 1 12;GET 15;");

    int32_t err = send_cmds(fd, test);
    if(err) goto L_DONE;

    err = query(fd, "hello2");
    if(err) goto L_DONE;

    err = query(fd, "hello3");

L_DONE:
    close(fd);
    return 0;
}
