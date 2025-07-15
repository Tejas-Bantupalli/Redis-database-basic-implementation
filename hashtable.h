#pragma once

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

struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0; // cached hash value
};

struct HTab {
    HNode **tab = NULL;
    size_t size = 0;
    size_t mask = 0;
};

struct HMap {
    HTab ht1; // newer
    HTab ht2; // older
    size_t resizing_pos = 0;
};

const size_t k_max_load_factor = 8;

const size_t k_resizing_work = 128;

void hm_insert(HMap *hmap, HNode *node);
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
HNode *hm_delete(HMap *hmap, HNode *key);
