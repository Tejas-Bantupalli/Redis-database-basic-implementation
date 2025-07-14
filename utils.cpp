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



void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }
    flags |= O_NONBLOCK;
    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}
bool try_flush_buffer(Conn *conn) {
    ssize_t rv;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        return false; // No space available, return false to indicate no more space

    }
    if (rv < 0) {
        perror("write()");
        conn->state = STATE_END; // Mark the connection for deletion
        return false; // Indicate an error
    }
    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size) {
        conn->state = STATE_REQ; // Transition back to request state
        conn->wbuf_size = 0; // Reset the write buffer size
        conn->wbuf_sent = 0; // Reset the write buffer sent size
        return false;
    }
    return true;
}
void stateres(Conn *conn) {
    while (try_flush_buffer(conn)){}
}

bool one_request(Conn *conn) {
    // Try to parse a request from the buffer
    if (conn->rbuf_size < 4) {
        // Not enough data in the buffer. Will retry in the next iteration
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if (len > k_max_msg) {
        printf("too long");
        conn->state = STATE_END;
        return false;
    }
    if (4 + len > conn->rbuf_size) {
        return false;
    }
    uint32_t rescode = 0;
    uint32_t wlen = 0;
    int32_t err = do_request(&conn->rbuf[4], len, &rescode, &conn->wbuf[4], &wlen);
    if (err != RES_OK) {
        printf("error in request processing");
        conn->state = STATE_END; // Mark the connection for deletion
        return false; // Indicate an error
    }
    // Write response length (wlen + 4 for rescode) at the start
    uint32_t total_len = wlen + 4;
    memcpy(&conn->wbuf[0], &total_len, 4);
    // Write response code
    memcpy(&conn->wbuf[4], &rescode, 4);
    // Write response data (if any)
    if (wlen > 0) {
        memcpy(&conn->wbuf[8], &conn->wbuf[4], wlen);
    }
    conn->wbuf_size = 4 + 4 + wlen; // 4 for length, 4 for rescode, wlen for data
    printf("client says: %.*s\n", len, &conn->rbuf[4]);
    size_t remain = conn->rbuf_size - 4 - len;
    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;
    conn->state = STATE_RES;
    stateres(conn);
    return (conn->state == STATE_REQ);
}


bool try_fill_buffer(Conn *conn) {
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv =0;
    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size],cap);
    } while (rv < 0 && errno == EINTR);
    if (rv<0 and errno == EAGAIN) {
        return false; // No data available, return false to indicate no more data
    }
    if (rv<=0) {
        perror("read()");
        conn->state = STATE_END; // Mark the connection for deletion
        return false; // Indicate an error
    }
    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));
    while (one_request(conn)) {
        // Process the request
    }
    return (conn->state==STATE_REQ); // Return true if still in request state
}

void statereq(Conn *conn) {
    while (try_fill_buffer(conn)){}
}





void connection_io(Conn *conn){
    if (conn->state == STATE_REQ){
        statereq(conn);
    }
    else if (conn->state == STATE_RES){
        stateres(conn);
    }
    else {
        assert(0);
    }
}

