#include <cstdio>        // for printf, perror
#include <cstdlib>       // for exit
#include <cstring>       // for memset, strlen
#include <unistd.h>      // for read, write, close
#include <sys/socket.h>  // for socket, connect
#include <netinet/in.h>  // for sockaddr_in, htons, htonl
#include <arpa/inet.h>   // for inet_ntop (if needed)
#include <sys/types.h>  // for ssize_t
#include <cstdint>       // for uint32_t
#include <cassert>       // for assert
#include "common.h"

static int32_t query(int fd, const char *text){
    uint32_t len = (uint32_t)strlen(text);
    if (len> k_max_msg){
        return -1;
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    int32_t errr = write_all(fd, wbuf, 4 + len);
    if (errr < 0) {
        return errr;
    }
    char rbuf[4 + k_max_msg+1];
    int32_t err = read_full(fd, rbuf, 4);
    if (err==-1) {

        die("stuff stuff");
        return err; // Indicate an error

    }
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        die("message too long");
        return -1; // Indicate an error
    }
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        die("read_full() error");
        return err;
    }
    rbuf[4 + len] = '\0'; // Null-terminate the string for printing
    printf("server says: %s\n", &rbuf[4]);
    return 0; // Indicate success


}






int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1 (localhost)

    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    if (fd < 0) {
        die("socket()");
    }

    int32_t err = query(fd, "hello1");
    if (err==-1){
        goto L_DONE;
    }
    err = query(fd, "hello2");
    if (err==-1){
        goto L_DONE;
    }
    err = query(fd, "hello3");
    if (err==-1){
        goto L_DONE;
    }
L_DONE:
    close(fd);
    return 0;
}
