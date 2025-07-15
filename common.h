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
};
static struct {
    HMap db;
} g_data;
int32_t do_request(const uint8_t *data, uint32_t len, uint32_t *rescode, uint8_t *res, uint32_t *reslen);
uint32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen);
uint32_t do_set(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen);
uint32_t do_del(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen);
int32_t parse_req(const uint8_t *data, size_t len, std::vector<std::string> &cmd);
struct Entry {
    struct HNode node;
    std::string key;
    std::string val;
};
// Portable C++ version of container_of macro
#include <cstddef>
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr)-offsetof(type, member)))

// entry_eq macro removed; use the function version in common.cpp
bool entry_eq(HNode *lhs, HNode *rhs);
    
    