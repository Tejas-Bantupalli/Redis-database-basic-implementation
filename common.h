#pragma once
#include <cstdint>
#include <cstdlib>

const size_t k_max_msg = 4096; // Maximum message size
int32_t read_full(int fd, char* buf, size_t len);
int32_t write_all(int fd, const char* buf, size_t len);
void die(const char* msg);
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2, // mark the connection for deletion
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