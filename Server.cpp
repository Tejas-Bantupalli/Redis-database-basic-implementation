#include <cstdio>        // for printf, perror
#include <cstdlib>       // for exit
#include <cstring>       // for memset, strlen
#include <unistd.h>      // for read, write, close
#include <sys/socket.h>  // for socket, setsockopt, bind, listen, accept
#include <netinet/in.h>  // for sockaddr_in, htons, htonl
#include <csignal>       // for signal (optional cleanup handling)
#include <cassert>       // for assert
#include <cstdint>       // for uint32_t
#include <arpa/inet.h>   // for inet_ntop (if needed)
#include <sys/types.h>  // for ssize_t
#include "common.h"
void msg(const char* s) {
    perror(s);
}

static int32_t one_request(int connfd) {
    char rbuf[4+ k_max_msg];
    int32_t err = read_full(connfd, rbuf, 4);
    if (err==-1) {
        msg("stuff");
        return err; // Indicate an error
    }
    uint32_t len =0;
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        msg("message too long");
        return -1; // Indicate an error
    }
    err = read_full(connfd, &rbuf[4], len);
    if (err){
        msg("read_full() error");
        return err;
    }
    rbuf[4 + len] = '\0'; // Null-terminate the string for printingprin
    printf("client says: %s\n", &rbuf[4]);
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len); // Return the result of write_all

}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);              // CORRECTED: htons, not ntohs
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // CORRECTED: htonl(0) -> htonl(INADDR_ANY)

    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            msg("accept() failed");
            continue;
        }
        while (true){
            int32_t err = one_request(connfd);
            if (err < 0) {
                break; 
            }
        }
        close(connfd);
    }

    close(fd);
    return 0;
}
