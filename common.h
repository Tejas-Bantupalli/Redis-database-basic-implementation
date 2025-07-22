#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <csignal>
#include <cassert>
#include <arpa/inet.h>
#include <sys/types.h>
#include <map>
#include <string>
#include <vector>
#include <poll.h>
#include <fcntl.h>
#include <sys/select.h>
#include "hashtable.h"
#include "zset.h"
#include "DList.h"
#include "heap.h"
#include "timer.h"
#include "serialisation.h"
#include "thread.h"

#define k_max_args 4
const size_t k_max_msg = 4096; // Maximum message size
int32_t read_full(int fd, char* buf, size_t len);
int32_t write_all(int fd, const char* buf, size_t len);
void die(const char* msg);
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2, // mark the connection for deletion
};
enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};
struct Conn {
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];
    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
    uint64_t idle_start = 0;
    DList idle_list;
};
struct GlobalData {
    HMap db;
    ZSet zset;
    std::vector<Conn *> fd2conn;
    DList idle_list;
    std::vector<HeapItem> heap;
    ThreadPool tp;
};

extern GlobalData g_data;
int32_t do_request(std::vector<std::string> &cmd, std::string &out);
uint32_t do_get(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_set(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_del(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_keys(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_zadd(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_zscore(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_zrem(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_query(const std::vector<std::string> &cmd, std::string &out);
uint32_t do_expire(std::vector<std::string> &cmd, std::string &out);
uint32_t do_ttl(std::vector<std::string> &cmd, std::string &out);
int32_t parse_req(const uint8_t *data, size_t len, std::vector<std::string> &cmd);
struct Entry {
    struct HNode node;
    std::string key;
    std::string val;
    uint32_t type = 0;
    ZSet *zset = NULL;
    size_t heap_idx = -1;
};
// Portable C++ version of container_of macro
#include <cstddef>
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr)-offsetof(type, member)))

// entry_eq macro removed; use the function version in common.cpp
bool entry_eq(HNode *lhs, HNode *rhs);
bool str2int(const std::string &s, int64_t &out);
void entry_del(Entry *ent);
    
    