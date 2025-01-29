#include <cassert>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <map>
#include <signal.h>
#include <cstring>  // For memcpy

#define PORT 9090
#define MAX_MSG_SIZE 4096
#define MAX_ARGS 10
#define container_of(ptr, T, member) \
    (T *)( (char *)ptr - offsetof(T, member) )

volatile sig_atomic_t keep_running = 1;

const size_t k_max_load_factor = 8;
const size_t k_max_msg = 4096;
const size_t k_resizing_work = 128; // constant work

enum {
    SER_NIL = 0, // Like NULL
    SER_ERR = 1, // An error code and message
    SER_STR = 2, // A string
    SER_INT = 3, // A int64
    SER_ARR = 4, // Array
    RES_OK = 0,  // Success
    RES_ERR = 1, // Error
    RES_NX = 2,  // Not found
};

// Hashtable node
struct HNode {
    HNode *next = nullptr;
    uint64_t hcode = 0; // cached hash value
};

// Hashtable
struct HTab {
    HNode **tab = nullptr; // array of HNode *
    size_t mask = 0; // 2^n - 1
    size_t size = 0;
};

// The real hashtable interface
struct HMap {
    HTab ht1; // newer
    HTab ht2; // older
    size_t resizing_pos = 0;
};

// Hashtable insertion
static void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask; // slot index
    HNode *next = htab->tab[pos]; // prepend the list
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

// n must be a power of 2
static void h_init(HTab *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

// Hashtable cleanup
static void h_free(HTab *htab) {
    if (htab->tab) {
        free(htab->tab);
        htab->tab = nullptr;
    }
    htab->size = 0;
}

HMap hmap;

// Parse request
static int32_t parse_req(const uint8_t *data, size_t len, std::vector<std::string> &out) {
    if (len < 8) return -1;
    uint32_t n = 0;
    memcpy(&n, &data[4], 4);
    n = ntohl(n);  // Convert from network byte order
    if (n > MAX_ARGS) return -1;
    size_t pos = 8;
    while (n--) {
        if (pos + 4 > len) return -1;
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4);
        sz = ntohl(sz);  // Convert from network byte order
        if (pos + 4 + sz > len) return -1;
        out.emplace_back(reinterpret_cast<const char*>(&data[pos + 4]), sz);
        pos += 4 + sz;
    }
    return 0;
}

// Handle query
static int32_t handle_query(const std::vector<std::string> &cmd, std::string &res) {
    if (cmd.empty()) {
        res = "(err) 1 No command received";
        return RES_ERR;
    }

    const std::string &action = cmd[0];

    if (action == "set" && cmd.size() == 3) {
        const std::string &key = cmd[1];
        const std::string &value = cmd[2];

        // Create a new HNode for the key-value pair
        HNode *node = new HNode;
        node->hcode = std::hash<std::string>{}(key);  // Generate hash code for the key
        // Store the key and value in the node (you might need to store them as needed)
        node->next = nullptr;  // Make sure next is null since it's a new node

        // Insert into ht1
        h_insert(&hmap.ht1, node);

        res = "(nil)";
        return RES_OK;
    } else if (action == "get" && cmd.size() == 2) {
        const std::string &key = cmd[1];

        // Lookup the key in the hash map
        HNode lookup_node;
        lookup_node.hcode = std::hash<std::string>{}(key);  // Hash the key
        HNode **node_ptr = h_lookup(&hmap.ht1, &lookup_node, [](HNode *a, HNode *b) { return true; });

        if (node_ptr) {
            res = "(str) " + std::string(reinterpret_cast<const char*>(&(*node_ptr)->hcode)); // Assuming you store value in the node
            return RES_OK;
        }
        res = "(nil)";
        return RES_OK;
    } else if (action == "del" && cmd.size() == 2) {
        const std::string &key = cmd[1];

        // Delete the key from the hash map
        HNode lookup_node;
        lookup_node.hcode = std::hash<std::string>{}(key);  // Hash the key
        HNode **node_ptr = h_lookup(&hmap.ht1, &lookup_node, [](HNode *a, HNode *b) { return true; });

        if (node_ptr) {
            h_detach(&hmap.ht1, node_ptr);  // Remove the node from ht1
            res = "(int) 1";  // Successfully deleted
        } else {
            res = "(int) 0";  // No such key to delete
        }
        return RES_OK;
    } else if (action == "keys" && cmd.size() == 1) {
        res = "(arr) len=" + std::to_string(hmap.ht1.size); // Use ht1's size to get the number of keys
        for (size_t i = 0; i <= hmap.ht1.mask; i++) {
            HNode *node = hmap.ht1.tab[i];
            while (node) {
                // You need to implement some logic to extract the key from the node
                res += " (str) " + std::to_string(node->hcode); // Assuming key is in node->hcode (or store the key elsewhere)
                node = node->next;
            }
        }
        res += " (arr) end";
        return RES_OK;
    } else {
        res = "(err) 1 Unknown cmd";
        return RES_ERR;
    }
}

// Send response
static int32_t send_res(int fd, const std::string &res) {
    ssize_t total_sent = 0;
    ssize_t len = res.length();
    const char *buf = res.c_str();

    while (total_sent < len) {
        ssize_t sent = send(fd, buf + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

// Remove a node from the chain
static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

// Lookup in hashtable
static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return nullptr;
    }
    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos]; // incoming pointer to the result
    for (HNode *cur; (cur = *from) != nullptr; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;
        }
    }
    return nullptr;
}

// Help resizing
static void hm_help_resizing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_resizing_work && hmap->ht2.size > 0) {
        HNode **from = &hmap->ht2.tab[hmap->resizing_pos];
        if (!*from) {
            hmap->resizing_pos++;
            continue;
        }
        h_insert(&hmap->ht1, h_detach(&hmap->ht2, from));
        nwork++;
    }
    if (hmap->ht2.size == 0) {
        h_free(&hmap->ht1);
        hmap->ht1 = hmap->ht2;
        hmap->ht2.tab = nullptr;
        std::cerr << "Resizing finished" << std::endl;
    }
}

// Initialize map
static void hm_start_resizing(HMap *hmap) {
    h_init(&hmap->ht2, hmap->ht1.size * 2);
}

// Reset
void reset() {
    if (hmap.ht1.tab) h_free(&hmap.ht1);
    if (hmap.ht2.tab) h_free(&hmap.ht2);
    h_init(&hmap.ht1, 32);
    hmap.ht2.tab = nullptr;
}

// Signal handler for SIGINT
void signal_handler(int sig) {
    keep_running = 0;
    reset();
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);  // Setup signal handler for cleanup

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    assert(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(fd, 10) == 0);
    
    while (keep_running) {
        struct pollfd fds[1] = {{fd, POLLIN, 0}};
        int ret = poll(fds, 1, 500);
        if (ret == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(fd, nullptr, nullptr);
            assert(client_fd >= 0);
            std::string res;
            while (keep_running) {
                uint8_t buf[k_max_msg];
                ssize_t len = recv(client_fd, buf, sizeof(buf), 0);
                if (len <= 0) break;
                
                std::vector<std::string> cmd;
                if (parse_req(buf, len, cmd) == 0) {
                    if (handle_query(cmd, res) == RES_OK) {
                        send_res(client_fd, res);
                    }
                }
            }
            close(client_fd);
        }
    }

    close(fd);
    return 0;
}
