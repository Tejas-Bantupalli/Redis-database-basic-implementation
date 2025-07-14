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

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;
    for (const std::string &arg : cmd) {
        len += 4 + (uint32_t)arg.size();
        if (len > k_max_msg) {
            return -1;
        }
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4); // total length
    uint32_t n = cmd.size();
    memcpy(&wbuf[4], &n, 4); // number of arguments
    size_t cur = 8;
    for (const std::string &arg : cmd) {
        uint32_t arglen = (uint32_t)arg.size();
        memcpy(&wbuf[cur], &arglen, 4);
        memcpy(&wbuf[cur + 4], arg.data(), arglen);
        cur += 4 + arglen;
    }
    int32_t err = write_all(fd, wbuf, 4 + len);
    return err;
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
    uint32_t rescode = 0;
    if (len < 4) {
        printf("bad response");
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);
    printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
    return 0;
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

int main(int argc, char **argv) {
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

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_resp(fd);
    if (err) {
        goto L_DONE;
    }
    L_DONE:
    close(fd);
    return 0;
}