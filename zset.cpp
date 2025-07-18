#include "zset.h"
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
#include "AVL.h"
#include "utils.h"

// Comparison function for AVL tree nodes based on score (and optionally name for tie-breaking)
bool zless(const AVLNode *a, const AVLNode *b) {
    const ZNode *za = container_of(a, ZNode, tnode);
    const ZNode *zb = container_of(b, ZNode, tnode);
    if (za->score != zb->score) {
        return za->score < zb->score;
    }
    // Tie-breaker: compare names lexicographically
    size_t min_len = za->len < zb->len ? za->len : zb->len;
    int cmp = memcmp(za->name, zb->name, min_len);
    if (cmp != 0) {
        return cmp < 0;
    }
    return za->len < zb->len;
}

ZNode *znode_new(const char *name, size_t len, double score) {
    ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len);
    avl_init(&node->tnode);
    node->hnode.next = NULL;
    node->hnode.hcode = str_hash((uint8_t *)name, len);
    node->score = score;
    node->len = len;
    memcpy(&node->name[0],name,len);
    return node;
}

bool hcmp(HNode *node, HNode *key) {
    ZNode *znode = container_of(node, ZNode, hnode);
    HKey *hkey = container_of(key,HKey,node);
    if (znode->len != hkey->len) {
        return false;
    }
    return 0 == memcmp(znode->name, hkey->name, znode->len);
}



ZNode *zset_lookup(ZSet *zset, const char *name, size_t len) {
    if (!zset-> tree) {
        return NULL;
    }
    HKey key;
    key.node.hcode = str_hash((uint8_t *)name, len);
    key.name = name;
    key.len = len;
    HNode *found = hm_lookup(&zset->hmap,&key.node,&hcmp);
    return found ? container_of(found,ZNode,hnode) : NULL;
}

void tree_add(ZSet *zset, ZNode *node) {
    AVLNode *cur = NULL;
    AVLNode **from = &zset->tree;
    while (*from) {
        cur = *from;
        from = zless(&node->tnode,cur) ? &cur->left : &cur->right;
    }
    *from = &node->tnode;
    node->tnode.parent = cur;
    zset->tree = avl_fix(&node->tnode);
}

void zset_update(ZSet *zset, ZNode *node,double score) {
    if (node->score == score) {
        return;
    }
    zset->tree = avl_del(&node->tnode);
    node->score = score;
    avl_init(&node->tnode);
    tree_add(zset,node);
}

bool zset_add(ZSet *zset, const char *name, size_t len, double score) {
    ZNode *node = zset_lookup(zset,name,len);
    if (node) {
        zset_update(zset, node, score);
        return false;
    }
    else {
        node = znode_new(name,len,score);
        hm_insert(&zset->hmap, &node->hnode);
        tree_add(zset,node);
        return true;
    }
}

ZNode *zset_pop(ZSet *zset, const char *name, size_t len) {
    ZNode *node = zset_lookup(zset,name,len);
    if (!node) {
        return NULL;
    }
    zset->tree = avl_del(&node->tnode);
    hm_delete(&zset->hmap, &node->hnode);
    return node;
}

void znode_del(ZNode *node){
    free(node);
}

ZNode *zset_query(ZSet *zset, double score, const char *name, size_t len) {
    AVLNode *found = NULL;

    // Create a temporary ZNode for comparison
    ZNode temp;
    avl_init(&temp.tnode);
    temp.score = score;
    temp.len = len;
    memcpy(temp.name, name, len);

    for (AVLNode *cur = zset->tree; cur;) {
        if (zless(cur, &temp.tnode)) {
            cur = cur->right;
        }
        else {
            found = cur;
            cur = cur->left;
        }
    }
    return found ? container_of(found, ZNode, tnode) : NULL;
}

AVLNode avl_offset(AVLNode *node, double offset) {
    int64_t pos = 0;
    while (pos!=offset) {
        if (pos<offset & pos + avl_count(node->right)>=offset) {
            node = node->right;
            pos += avl_count(node->left) + 1;
        }
        else if (pos>offset & pos - avl_count(node->left)<=offset) {
            node = node->left;
            pos -= avl_count(node->right) + 1;
        }
        else {
            AVLNode *parent = node->parent;
            if (!parent) {
                return NULL;
            }
            if (parent->right == node) {
                pos -= avl_count(node->left) + 1;
            }
            else{
                pos+=avl_count(node->right)+1;
            }
            node->parent;
        }
    }
    return node;
}


ZNode *znode_offset(ZNode *node, int64_t offset) {
    AVLNode *tnode = node ? avl_offset(&node->tnode, offset) : NULL;
    return tnode ? container_of(tnode, ZNode, tnode) : NULL;
}

