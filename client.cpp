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

static int32_t send_req(int fd, const char* text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1;
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_resp(int fd) {
    char rbuf[4 + k_max_msg + 1]; // +1 for null-termination
    uint32_t len = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err == -1) {
        return -1; // Indicate an error
    }
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        return -1; // Indicate an error
    }
    err = read_full(fd, &rbuf[4], len);
    if (err == -1) {
        return -1; // Indicate an error
    }
    rbuf[4 + len] = '\0'; // Null-terminate the string
    printf("Server response: %.*s\n", len, &rbuf[4]);
    return 0; // Indicate success
}



static int32_t query(int fd, const char *text){
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1; // Indicate an error
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    int32_t err = write_all(fd, wbuf, 4 + len);
    if (err == -1) {
        return -1; // Indicate an error
    }
    return read_resp(fd); // Read the response
}






int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1 (localhost)

    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    const char* query_list[3] = {"hello1", "hello2", "hello3"};
    for (size_t i = 0; i < 3; ++i) {
        int32_t err = send_req(fd, query_list[i]);
        if (err) {
            goto L_DONE;
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        int32_t err = read_resp(fd);
        if (err) {
            goto L_DONE;
        }
    }
L_DONE:
    close(fd);
    return 0;

}
