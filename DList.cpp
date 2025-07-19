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
#include "DList.h"

void dList_init(DList *node) {
    node->prev = node;
    node->next = node;
}

bool dList_empty(DList *node) {
    return node->next == node || node->next == nullptr;
}

void dlist_detach(DList *node) {
    DList *prev = node->prev;
    DList *next = node->next;
    prev->next = next;
    next -> prev = prev;
}

void list_insert_before(DList *target, DList *node) {
    DList *prev = target->prev;
    prev->next = node;
    node->prev = prev;
    node->next = target;
    target->prev = node;
}