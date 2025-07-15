// common.cpp
#include "common.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include "hashtable.h"
#include "utils.h"
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

bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

uint32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node){
        return RES_NX;
    }
    const std::string &value =  container_of(node,Entry,node)->val;
    assert(value.size() <= k_max_msg);
    memcpy(res, value.data(), value.size());
    *reslen = (uint32_t)value.size();
    return RES_OK;
}

uint32_t do_set(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    Entry* entry = new Entry;
    entry->key = cmd[1];
    entry->val = cmd[2];
    entry->node.hcode = str_hash((const uint8_t*)entry->key.data(), entry->key.size());
    printf("[DEBUG] Allocating Entry at %p for key '%s'\n", (void*)entry, entry->key.c_str());
    hm_insert(&g_data.db, &entry->node);
    // Echo the value back in the response
    size_t vlen = entry->val.size();
    if (vlen > k_max_msg) vlen = k_max_msg;
    memcpy(res, entry->val.data(), vlen);
    *reslen = (uint32_t)vlen;
    return RES_OK;
}
//

uint32_t do_del(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen) {
    (void)res;
    (void)reslen;
    // Create a temporary key for lookup only; do not insert stack-allocated Entry into the hash table
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((const uint8_t*)key.key.data(), key.key.size());
    HNode* node = hm_delete(&g_data.db, &key.node);
    if (node) {
        // Only delete if node points to a heap-allocated Entry (which should always be the case)
        Entry* entry = container_of(node, Entry, node);
        printf("[DEBUG] Deleting Entry at %p for key '%s'\n", (void*)entry, entry->key.c_str());
        delete entry;
        node = nullptr; // Extra safety: avoid dangling pointer
    }
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

