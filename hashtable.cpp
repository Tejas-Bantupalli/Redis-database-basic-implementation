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
#include "hashtable.h"

static void hinit(HTab *ht, size_t size) {
    assert(size > 0 && (size & (size - 1)) == 0); // size must be a power of 2
    ht->tab = (HNode **)calloc(size, sizeof(HNode *));
    ht->size = 0;
    ht->mask = size - 1;
}

static void hinsert(HTab *ht, HNode *node) {
    assert(node != NULL);
    size_t index = node->hcode & ht->mask;
    HNode *next = ht->tab[index];
    node->next = next;
    ht->tab[index] = node;
    ht->size++;
}

static HNode **hlookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return NULL;
    }
    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];
    for (HNode *cur;  (cur=*from)!=NULL; from = &cur->next){
        if (cur->hcode == key->hcode &&eq(cur,key)) {
            return from;
        }
    }
    return NULL;
}

static HNode* h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

static void hm_start_resizing(HMap *hmap) {
    assert(hmap->ht2.tab == NULL);
    hmap->ht2 = hmap->ht1;
    hinit(&hmap->ht1, (hmap->ht1.mask + 1) * 2);
    hmap->resizing_pos = 0;
}

static void hm_help_resizing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_resizing_work && hmap->ht2.size>0) {
        HNode **from = &hmap->ht2.tab[hmap->resizing_pos];
        if (!*from) {
            hmap->resizing_pos++;
            continue;
        }
        hinsert(&hmap->ht1,h_detach(&hmap->ht2, from));
        nwork++;
    }
    if ((hmap->ht2.size ==0) && (hmap->ht2.tab)) {
        free(hmap->ht2.tab);
        hmap->ht2 = HTab{};
    }
}

void hm_insert(HMap *hmap, HNode *node) {
    printf("[DEBUG] hm_insert: inserting node at %p\n", (void*)node);
    if (!hmap->ht1.tab){
        hinit(&hmap->ht1,4);
    }
    hinsert(&hmap->ht1,node);
    if (!hmap->ht2.tab) {
        size_t load_factor = hmap->ht1.size / (hmap->ht1.mask + 1);
        if (load_factor>k_max_load_factor) {
            hm_start_resizing(hmap);
        }

    }
    hm_help_resizing(hmap);
}



HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_resizing(hmap);
    HNode **from = hlookup(&hmap->ht1, key, eq);
    from = from ? from : hlookup(&hmap->ht2, key, eq);
    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key) {
    hm_help_resizing(hmap);
    HNode **from = hlookup(&hmap->ht1, key, entry_eq);
    if (!from) from = hlookup(&hmap->ht2, key, entry_eq);
    return from ? h_detach(&hmap->ht1, from) : NULL;
}
