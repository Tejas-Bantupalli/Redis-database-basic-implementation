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

// Global map
static std::map<std::string, std::string> g_map;

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

// Handle query// In the server code, modify the handle_query function:

static int32_t handle_query(const std::vector<std::string> &cmd, std::string &res) {
    if (cmd.empty()) {
        res = "(err) 1 No command received";
        return RES_ERR;
    }

    const std::string &action = cmd[0];

    if (action == "set" && cmd.size() == 3) {
        const std::string &key = cmd[1];
        const std::string &value = cmd[2];
        g_map[key] = value;
        res = "(nil)";
        return RES_OK;
    } else if (action == "get" && cmd.size() == 2) {
        const std::string &key = cmd[1];
        auto it = g_map.find(key);
        if (it != g_map.end()) {
            res = "(str) " + it->second;
            return RES_OK;
        }
        res = "(nil)";
        return RES_OK;  // Changed from RES_NX to RES_OK
    } else if (action == "del" && cmd.size() == 2) {
        const std::string &key = cmd[1];
        size_t count = g_map.erase(key);
        res = "(int) " + std::to_string(count);
        return RES_OK;
    } else if (action == "keys" && cmd.size() == 1) {
        res = "(arr) len=" + std::to_string(g_map.size());
        for (const auto &pair : g_map) {
            res += " (str) " + pair.first;
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

// The real hashtable interface
struct HMap {
    HTab ht1; // newer
    HTab ht2; // older
    size_t resizing_pos = 0;
};

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
    assert(hmap->ht2.tab == nullptr);
    size_t n = hmap->ht1.mask + 1;
    h_init(&hmap->ht2, n * 2);
    std::cerr << "Resizing started" << std::endl;
}

// Cleanup map
static void hm_cleanup(HMap *hmap) {
    h_free(&hmap->ht1);
    h_free(&hmap->ht2);
}

// Signal handler
static void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        keep_running = 0;
    }
}




int main() {
    // Signal handling setup
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Server setup
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cerr << "Server started on port " << PORT << std::endl;

    std::vector<pollfd> poll_fds;
    poll_fds.push_back({server_fd, POLLIN, 0});

    HMap hmap;
    h_init(&hmap.ht1, 16);

    while (keep_running) {
        int poll_count = poll(poll_fds.data(), poll_fds.size(), 5000);
        if (poll_count < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        for (size_t i = 0; i < poll_fds.size(); i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == server_fd) {
                    // Accept new connections
                    sockaddr_in client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (client_fd < 0) {
                        perror("accept");
                        continue;
                    }
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);
                    poll_fds.push_back({client_fd, POLLIN, 0});
                } else {
                    // Handle client requests
                    int client_fd = poll_fds[i].fd;
                    uint8_t buffer[MAX_MSG_SIZE];
                    ssize_t read_size = read(client_fd, buffer, sizeof(buffer));
                    if (read_size < 0) {
                        if (errno == EINTR) continue;
                        perror("read");
                        close(client_fd);
                        continue;
                    }
                    if (read_size == 0) {
                        close(client_fd);
                        continue;
                    }

                    std::vector<std::string> cmd;
                    std::string response;
                    if (parse_req(buffer, read_size, cmd) == 0) {
                        handle_query(cmd, response);
                        send_res(client_fd, response);
                    } else {
                        send_res(client_fd, "(err) 1 Invalid request");
                    }
                }
            }
        }

        // Handle resizing if needed
        if (hmap.ht2.tab) {
            hm_help_resizing(&hmap);
        }
    }

    std::cerr << "Server shutting down" << std::endl;
    hm_cleanup(&hmap);
    close(server_fd);
    return 0;
}