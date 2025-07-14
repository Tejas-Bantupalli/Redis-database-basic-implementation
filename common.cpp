// common.cpp
#include "common.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>

void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int32_t read_full(int fd, char* buf, size_t len) {
    while (len > 0) {
        ssize_t rv = read(fd, buf, len);
        if (rv <= 0) return -1;
        assert((size_t)rv <= len);
        buf += rv;
        len -= rv;
    }
    return 0;
}

int32_t write_all(int fd, const char* buf, size_t len) {
    while (len > 0) {
        ssize_t rv = write(fd, buf, len);
        if (rv <= 0) return -1;
        assert((size_t)rv <= len);
        buf += rv;
        len -= rv;
    }
    return 0;
}
