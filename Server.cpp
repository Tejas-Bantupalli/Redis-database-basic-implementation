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
#include "DList.h"
#include "timer.h"

static int32_t accept_new_connection(int fd, std::vector<Conn *> &fd2conn) {
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
    // Initialize the connection structure
    memset(conn, 0, sizeof(Conn));
    conn->fd = new_fd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn->idle_start = get_monotonic_usec();
    
    // Initialize the idle_list before inserting it
    dList_init(&conn->idle_list);
    list_insert_before(&g_data.idle_list, &conn->idle_list);
    conn_put(fd2conn, conn);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    dList_init(&g_data.idle_list);
    thread_pool_init(&g_data.tp, 4);

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
    fd_set_nb(fd); // Set the socket to non-blocking mode
    std::vector<struct pollfd> poll_args;
    while (true) {
        poll_args.clear();
        
        // Add the listening socket
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        
        // Add all client connections
        for (Conn *conn : g_data.fd2conn) {
            if (!conn) continue; // Skip null pointers
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }
        
        int timeout_ms = (int)next_timer_ms();
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout_ms);
        if (rv < 0) {
            die("poll()");
        }
        
        // Handle events
        for (size_t i = 0; i < poll_args.size(); i++) {
            if (poll_args[i].revents) {
                if (i == 0) {
                    // This is the listening socket
                    (void)accept_new_connection(fd, g_data.fd2conn);
                } else {
                    // This is a client connection
                    int client_fd = poll_args[i].fd;
                    if (client_fd < 0 || client_fd >= (int)g_data.fd2conn.size()) {
                        continue;
                    }
                    Conn *conn = g_data.fd2conn[client_fd];
                    if (!conn) {
                        continue;
                    }
                    
                    connection_io(conn);
                    if (conn->state == STATE_END) {
                        g_data.fd2conn[conn->fd] = NULL;
                        (void)close(conn->fd);
                        dlist_detach(&conn->idle_list);
                        free(conn);
                    }
                }
            }
        }
        
        // Process idle timeouts
        process_timers();
    }
    return 0;
}


