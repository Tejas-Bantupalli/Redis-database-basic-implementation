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
#include "timer.h"
#include "DList.h"
#include "common.h"

uint64_t get_monotonic_usec() {
    timespec tv = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return uint64_t(tv.tv_sec) * 1000000 + tv.tv_nsec / 1000;
}

uint32_t next_timer_ms() {
    if (dList_empty(&g_data.idle_list)) {
        return 10000;
    }
    uint64_t now_us = get_monotonic_usec();
    Conn *next = container_of(g_data.idle_list.next, Conn, idle_list);
    uint64_t next_us = next->idle_start + k_idle_timeout_ms * 1000;
    if (next_us <= now_us) {
        return 0;
    }
    return (uint32_t)((next_us - now_us) / 1000);
}

void process_timers() {
    uint64_t now_us = get_monotonic_usec();
    while (!dList_empty(&g_data.idle_list)) {
        Conn *next = container_of(g_data.idle_list.next,Conn,idle_list);
        uint64_t next_us = next->idle_start + k_idle_timeout_ms*1000;
        if (next_us > now_us) {
            break;
        }
        printf("removing idle connection: %d\n",next->fd);
        g_data.fd2conn[next->fd] = NULL;
        (void)close(next->fd); // Close the socket
        dlist_detach(&next->idle_list);
        free(next); // Free the memory
    }
    
}