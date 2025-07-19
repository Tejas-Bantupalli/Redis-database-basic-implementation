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
#include "zset.h"


void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int32_t read_full(int fd, char* buf, size_t len) {
    while (len > 0) {
        ssize_t rv = read(fd, buf, len);
        if (rv <= 0) {
            if (rv == 0) {
                // EOF - connection closed by peer
                return -1;
            }
            if (errno == EINTR) {
                continue; // Interrupted, try again
            }
            return -1; // Other error
        }
        assert((size_t)rv <= len);
        buf += rv;
        len -= rv;
    }
    return 0;
}

int32_t write_all(int fd, const char* buf, size_t len) {
    while (len > 0) {
        ssize_t rv = write(fd, buf, len);
        if (rv <= 0) {
            if (rv == 0) {
                // Write returned 0, which shouldn't happen normally
                return -1;
            }
            if (errno == EINTR) {
                continue; // Interrupted, try again
            }
            return -1; // Other error
        }
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
    else if (cmd[0] == "get" && cmd.size() == 2) {
        return do_get(cmd, out);
    } else if (cmd[0] == "set" && cmd.size() == 3) {
        return do_set(cmd, out);
    } else if (cmd[0] == "del" && cmd.size() == 2) {
        return do_del(cmd, out);
    } else if (cmd[0] == "zadd"){
        return do_zadd(cmd,out);
    }
    else if (cmd[0] == "zscore"){
        return do_zscore(cmd,out);
    }
    else if (cmd[0] == "zrem"){
        return do_zrem(cmd,out);
    }
    else if (cmd[0] == "zquery"){
        return do_query(cmd,out);
    }

    else {
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

uint32_t do_zadd(const std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() != 4) {
        out_err(out, RES_ERR, "Usage: zadd <key> <score> <name>");
        return RES_ERR;
    }
    double score = std::stod(cmd[2]);
    const std::string &name = cmd[3];
    // Add or update the element in the global zset
    bool added = zset_add(&g_data.zset, name.data(), name.size(), score);
    out_int(out, added ? 1 : 0); // 1 if new, 0 if updated
    return RES_OK;
}

uint32_t do_zscore(const std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() != 3) {
        out_err(out, RES_ERR, "Usage: zscore <key> <name>");
        return RES_ERR;
    }
    const std::string &name = cmd[2];
    ZNode *znode = zset_lookup(&g_data.zset, name.data(), name.size());
    if (!znode) {
        out_nil(out);
        return RES_NX;
    }
    out_dbl(out, znode->score);
    return RES_OK;
}

uint32_t do_zrem(const std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() != 3) {
        out_err(out, RES_ERR, "Usage: zrem <key> <name>");
        return RES_ERR;
    }
    const std::string &name = cmd[2];
    ZNode *znode = zset_pop(&g_data.zset, name.data(), name.size());
    if (znode) {
        out_int(out, 1);
    } else {
        out_int(out, 0);
    }
    return RES_OK;
}

uint32_t do_query(const std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() != 6) {
        out_err(out, RES_ERR, "Usage: zquery <key> <score> <name> <offset> <limit>");
        return RES_ERR;
    }
    double score = std::stod(cmd[2]);
    const std::string &name = cmd[3];
    int64_t offset = std::stoll(cmd[4]);
    int64_t limit = std::stoll(cmd[5]);
    ZNode *znode = zset_query(&g_data.zset, score, name.data(), name.size());
    znode = znode_offset(znode, offset);
    uint32_t n = 0;
    while (znode && n < (uint32_t)limit) {
        out_str(out, std::string(znode->name, znode->len));
        out_dbl(out, znode->score);
        znode = znode_offset(znode, +1); // successor
        n++;
    }
    return RES_OK;
}

// Global data definition
GlobalData g_data = {};