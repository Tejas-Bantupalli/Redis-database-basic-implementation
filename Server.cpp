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
#include <vector>
#include <poll.h>        // for poll
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <sys/select.h>
#include "utils.h"

static int32_t accept_new_connection(int fd, std::vector<Conn *> &f2dconn) {
    struct sockaddr_in addr = {};
    socklen_t addr_len = sizeof(addr);
    int new_fd = accept(fd, (struct sockaddr *)&addr, &addr_len);
    if (new_fd < 0) {
        perror("accept()");
        return -1;
    }
    Conn *conn = (Conn *)malloc(sizeof(Conn));
    if (!conn) {
        perror("malloc()");
        close(new_fd);
        return -1;
    }
    conn->fd = new_fd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    if (f2dconn.size() <= (size_t)new_fd) {
        f2dconn.resize(new_fd + 1, nullptr); // Resize to accommodate new_fd
    }
    f2dconn[new_fd] = conn;
    return 0;
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
    std::vector<Conn *> f2dconn;
    fd_set_nb(fd); // Set the socket to non-blocking mode
    std::vector<struct pollfd> poll_args;
    while (true) {
        poll_args.clear();
        struct pollfd pfd = {fd,POLLIN,0};
        poll_args.push_back(pfd);
        for (Conn *conn : f2dconn) {
            if (!conn) continue; // Skip null pointers
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll()");
        }
        for (size_t i =0;i< poll_args.size();i++){
            if (poll_args[i].revents){
                int fd = poll_args[i].fd;
                if (fd < 0 || fd >= (int)f2dconn.size()) continue; // skip invalid fds
                Conn *conn = f2dconn[fd];
                if (!conn) continue; // skip if no connection
                connection_io(conn);
                if (conn->state == STATE_END) {
                    f2dconn[conn->fd] = nullptr; // Mark for deletion
                    (void)close(conn->fd); // Close the socket
                    free(conn); // Free the memory
                }
            }
        }
        if (poll_args[0].revents) {
            (void)accept_new_connection(fd, f2dconn);
        }
        

    }
    return 0;
}


