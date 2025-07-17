// common.cpp
#include "common.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include "hashtable.h"
#include "utils.h"
#include "serialisation.h"
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
int32_t do_request(std::vector<std::string> &cmd, std::string &out) {
    if (cmd.empty()) {
        return RES_ERR; // Invalid command
    }
    if (cmd[0] == "keys" && cmd.size() == 1) {
        return do_keys(cmd, out);
    }
    if (cmd[0] == "get" && cmd.size() == 2) {
        return do_get(cmd, out);
    } else if (cmd[0] == "set" && cmd.size() == 3) {
        return do_set(cmd, out);
    } else if (cmd[0] == "del" && cmd.size() == 2) {
        return do_del(cmd, out);
    } else {
        return RES_ERR; // Unknown command
    }
}

bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

uint32_t do_get(const std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node) {
        printf("[DEBUG] do_get: RES_NX=%d\n", RES_NX);
        out_int(out, RES_NX); // Not found
        return RES_NX;
    }
    const std::string &value = container_of(node, Entry, node)->val;
    assert(value.size() <= k_max_msg);
    out_str(out, value);
    return RES_OK;
}

uint32_t do_set(const std::vector<std::string> &cmd, std::string &out) {
    Entry* entry = new Entry;
    entry->key = cmd[1];
    entry->val = cmd[2];
    entry->node.hcode = str_hash((const uint8_t*)entry->key.data(), entry->key.size());
    hm_insert(&g_data.db, &entry->node);
    printf("[DEBUG] do_set: RES_OK=%d\n", RES_OK);
    out_int(out, RES_OK);
    return RES_OK;
}

uint32_t do_del(const std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((const uint8_t*)key.key.data(), key.key.size());
    HNode* node = hm_delete(&g_data.db, &key.node);
    if (node) {
        delete container_of(node, Entry, node);
    }
    printf("[DEBUG] do_del: node? %d\n", node ? 1 : 0);
    out_int(out, node ? 1 : 0);
    return RES_OK;
}

uint32_t do_keys(const std::vector<std::string> &cmd, std::string &out) {
    (void)cmd;
    out_arr(out, (uint32_t)hm_size(&g_data.db));
    h_scan(&g_data.db.ht1, &cb_scan, &out);
    h_scan(&g_data.db.ht2, &cb_scan, &out);
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

