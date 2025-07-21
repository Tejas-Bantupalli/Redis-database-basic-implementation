
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
#include <vector>
#include <poll.h>        // for poll
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <sys/select.h>
#include "heap.h"

static size_t heap_parent(size_t i) {
    return (i + 1) / 2 - 1;
}
static size_t heap_left(size_t i) {
    return i * 2 + 1;
}
static size_t heap_right(size_t i) {
    return i * 2 + 2;
}

void heap_up(HeapItem *a, size_t pos) {
    HeapItem t = a[pos];
    assert(t.ref != NULL);
    while (pos>0 && a[heap_parent(pos)].val > t.val) {
        a[pos] = a[heap_parent(pos)];
        assert(a[pos].ref != NULL);
        *a[pos].ref = pos;
        pos = heap_parent(pos);
    }
    a[pos] = t;
    *a[pos].ref = pos;
}

void heap_down(HeapItem *a, size_t pos, size_t len) {
    HeapItem t = a[pos];
    assert(t.ref != NULL);
    while (true) {
        size_t l = heap_left(pos);
        size_t r = heap_right(pos);
        size_t min_pos = pos;
        uint64_t min_val = t.val;
        if (l < len && a[l].val < min_val) {
            min_pos = l;
            min_val = a[l].val;
        }
        if (r < len && a[r].val < min_val) {
            min_pos = r;
            min_val = a[r].val;
        }
        if (min_pos == pos) {
            break;
        }
        a[pos] = a[min_pos];
        assert(a[pos].ref != NULL);
        *a[pos].ref = pos;
        pos = min_pos;
    }
    a[pos] = t;
    *a[pos].ref = pos;
}

void heap_update(HeapItem *a, size_t pos, size_t len) {
    if (pos > 0 && a[heap_parent(pos)].val > a[pos].val) {
        heap_up(a, pos);
    } else {
        heap_down(a, pos, len);
}
}