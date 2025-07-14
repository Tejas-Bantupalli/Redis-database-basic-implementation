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
int32_t do_request(const uint8_t *data, uint32_t len, uint32_t *rescode, uint8_t *res, uint32_t *reslen) {
    std::vector<std::string> cmd;
    int32_t err = parse_req(data, len, cmd);
    if (err != 0) {
        return RES_ERR; // Indicate an error
    }
    if (cmd.empty()) {
        return RES_ERR; // Invalid command
    }
    if (cmd[0] == "get" && cmd.size() == 2) {
        return do_get(cmd, res, reslen);
    } else if (cmd[0] == "set" && cmd.size() == 3) {
        return do_set(cmd, res, reslen);
    } else if (cmd[0] == "del" && cmd.size() == 2) {
        return do_del(cmd, res, reslen);
    } else {
        return RES_ERR; // Unknown command
    }
    return RES_ERR; // Default case for error handling
}
uint32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    if (!g_map.count(cmd[1])) {
        return RES_NX;
    }
    std::string &value = g_map[cmd[1]];
    assert(value.size() <= k_max_msg);
    memcpy(res, value.data(), value.size());
    *reslen = (uint32_t)value.size();
    return RES_OK;
}

uint32_t do_set(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    (void)res; // Unused
    (void)reslen; // Unused
    g_map[cmd[1]] = cmd[2];
    return RES_OK;
}

uint32_t do_del(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    (void)res; // Unused
    (void)reslen; // Unused
    g_map.erase(cmd[1]);
    return RES_OK;
}

int32_t parse_req(const uint8_t  *data, size_t len, std::vector<std::string> &cmd) {
    if (len < 4) {
        return -1; // Invalid request
    }
    uint32_t cmd_len = 0;
    memcpy(&cmd_len, data, 4);
    if (cmd_len > k_max_args) {
        return -1; // Invalid request
    }
    size_t pos = 4;
    while(cmd_len--){
        if (pos+4>len){
            return -1; // Invalid request
        }
        uint32_t arg_len = 0;
        memcpy(&arg_len, &data[pos], 4);
        if (pos + 4 + arg_len > len) {
            return -1; // Invalid request
        }
        cmd.push_back(std::string((const char *)&data[pos + 4], arg_len));
        pos += 4 + arg_len;

    }
    if (pos != len) {
        return -1; // Invalid request
    }
    return 0; // Successfully parsed the request
}

