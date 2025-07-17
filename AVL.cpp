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
#include <algorithm>
#include "AVL.h"

void avl_init(AVLNode *node) {
    node -> left = node-> right = node-> parent = NULL;
    node->count = 1;
    node->depth = 1;
}

uint32_t avl_depth(AVLNode *node) {
    return node ? node->depth : 0;
}

uint32_t avl_count(AVLNode *node) {
    return node ? node->count : 0;
}
void avl_update(AVLNode *node) {
    node->depth = 1 + std::max(avl_depth(node->left), avl_depth(node->right));
    node->count = 1 + avl_count(node->left) + avl_count(node->right);
}

AVLNode *rot_left(AVLNode *node) {
    AVLNode *new_node = node->right;
    if (new_node->left) {
        new_node->left->parent = node;
    }
    node->right = new_node->left;
    new_node->left = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

AVLNode *rot_right(AVLNode *node) {
    AVLNode *new_node = node->left;
    if (new_node->right) {
        new_node->right->parent = node;
    }
    node->left = new_node->right;
    new_node->right = node;
    new_node->parent = node->parent;
    node->parent = new_node;
    avl_update(node);
    avl_update(new_node);
    return new_node;    
}

AVLNode *avl_fix_left(AVLNode *root) {
    if ((avl_depth(root->left->left)<avl_depth(root->left->right))) {
        root->left = rot_left(root->left);
    }
    return rot_right(root);
}

AVLNode *avl_fix_right(AVLNode *root) {
    if ((avl_depth(root->right->right)<avl_depth(root->right->left))) {
        root->right = rot_right(root->right);
    }
    return rot_left(root);
}

AVLNode *avl_fix(AVLNode *node) {
    while (true){
        avl_update(node);
        uint32_t l = avl_depth(node->left);
        uint32_t r = avl_depth(node->right);
        AVLNode *parent = node->parent;
        if (l==r+2){
            node = avl_fix_left(node);
        }
        else if (l+2==r) {
            node = avl_fix_right(node);
        } 
        if (!parent) {
            return node;
        }
        node = node->parent;
    }
}

AVLNode *avl_del(AVLNode *node) {
    if (node->right ==NULL) {
        AVLNode *parent = node->parent;
        if (node->left) {
            node->left->parent = parent;
        }
        if (parent) {
            (parent->left ==node ? parent->left : parent->right) = node->left;
            return avl_fix(parent);
        }
        else return node->left;
    }
}

static AVLNode *avl_del(AVLNode *node) {
    if (node->right == NULL) {
        // no right subtree, replace the node with the left subtree
        // link the left subtree to the parent
        AVLNode *parent = node->parent;
        if (node->left) {
            node->left->parent = parent;
        }
        if (parent) { // attach the left subtree to the parent
            (parent->left == node ? parent->left : parent->right) = node->left;
            return avl_fix(parent); // AVL-specific!
        } else { // removing root?
            return node->left;
        }
    } else {
        // detach the successor
        AVLNode *victim = node->right;
        while (victim->left) {
            victim = victim->left;
        }
        AVLNode *root = avl_del(victim);
        // swap with it
        *victim = *node;
        if (victim->left) {
            victim->left->parent = victim;
        }
        if (victim->right) {
            victim->right->parent = victim;
        }
        if (AVLNode *parent = node->parent) {
            (parent->left == node ? parent->left : parent->right) = victim;
            return root;
        } else { // removing root?
            return victim;
        }
    }
}