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
    uint8_t rbuf[K_MAX_MSG];
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
    
    ret = read(fd, rbuf, K_MAX_MSG);
    if (ret <= 0) {
        printf("read() error %d\n", errno);
        return ret;
    }
    
    uint8_t *rptr = rbuf;
    nstrs = *(uint8_t *)rptr;
    rptr += sizeof(uint8_t);
    printf("\n%d -> ",nstrs);
    for(int i =0; i < nstrs ;i++) {
        printf("%d:", *(int*)rptr);
        rptr+= sizeof(int);
        printf("%ul|", *(uint32_t*)rptr);
        rptr+= sizeof(uint32_t);
    }
    printf("\n");

    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    struct sockaddr_in addr = {};

    int fd = cerr(socket(AF_INET, SOCK_STREAM, 0));

    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    cerr(connect(fd, (const struct sockaddr *)&addr, sizeof(addr)));
    
    // mutlitple requests
    char test[80];
    strcpy(test, "SET 1 12;");

    int32_t err = send_cmds(fd, test);
    if(err) goto L_DONE;

L_DONE:
    close(fd);
    return 0;
}
