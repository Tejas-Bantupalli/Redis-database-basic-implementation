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
#include "heap.h"

GlobalData g_data;

void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void entry_set_ttl(Entry *ent, int64_t ttl_ms) {
    if (ttl_ms < 0 && ent->heap_idx != (size_t)-1) {
        size_t pos = ent->heap_idx;
        size_t last = g_data.heap.size() - 1;
        if (pos != last) {
            g_data.heap[pos] = g_data.heap[last];
            *g_data.heap[pos].ref = pos;
        }
        g_data.heap.pop_back();
        if (pos < g_data.heap.size()) {
            heap_update(g_data.heap.data(), pos, g_data.heap.size());
        }
        ent->heap_idx = -1;
    } else if (ttl_ms >= 0) {
        size_t pos = ent->heap_idx;
        if (pos == (size_t)-1) {
            HeapItem item;
            item.ref = &ent->heap_idx;
            g_data.heap.push_back(item);
            pos = g_data.heap.size() - 1;
            ent->heap_idx = pos;
        }
        g_data.heap[pos].val = get_monotonic_usec() + (uint64_t)ttl_ms * 1000;
        heap_update(g_data.heap.data(), pos, g_data.heap.size());
    }
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
    else if (cmd[0] == "expire" && cmd.size() == 3) {
        return do_expire(cmd, out);
    }
    else if (cmd[0] == "ttl" && cmd.size() == 2) {
        return do_ttl(cmd, out);
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
    entry->type = 0; // string type
    entry->zset = nullptr;
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
        entry_del(container_of(node, Entry, node));
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
    // Look up or create the zset entry in the DB
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((const uint8_t*)key.key.data(), key.key.size());
    HNode* node = hm_lookup(&g_data.db, &key.node, entry_eq);
    Entry* entry = nullptr;
    if (!node) {
        entry = new Entry;
        entry->key = cmd[1];
        entry->val = "";
        entry->type = 1; // ZSet type
        entry->zset = new ZSet();
        entry->node.hcode = key.node.hcode;
        hm_insert(&g_data.db, &entry->node);
    } else {
        entry = container_of(node, Entry, node);
    }
    bool added = zset_add(entry->zset, name.data(), name.size(), score);
    out_int(out, added ? 1 : 0); // 1 if new, 0 if updated
    return RES_OK;
}

uint32_t do_zscore(const std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() != 3) {
        out_err(out, RES_ERR, "Usage: zscore <key> <name>");
        return RES_ERR;
    }
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node) {
        out_nil(out);
        return RES_NX;
    }
    Entry *entry = container_of(node, Entry, node);
    if (entry->type != 1 || !entry->zset) {
        out_nil(out);
        return RES_NX;
    }
    const std::string &name = cmd[2];
    ZNode *znode = zset_lookup(entry->zset, name.data(), name.size());
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
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node) {
        out_int(out, 0);
        return RES_OK;
    }
    Entry *entry = container_of(node, Entry, node);
    if (entry->type != 1 || !entry->zset) {
        out_int(out, 0);
        return RES_OK;
    }
    const std::string &name = cmd[2];
    ZNode *znode = zset_pop(entry->zset, name.data(), name.size());
    if (znode) {
        znode_del(znode);
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
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node) {
        return RES_NX;
    }
    Entry *entry = container_of(node, Entry, node);
    if (entry->type != 1 || !entry->zset) {
        return RES_NX;
    }
    double score = std::stod(cmd[2]);
    const std::string &name = cmd[3];
    int64_t offset = std::stoll(cmd[4]);
    int64_t limit = std::stoll(cmd[5]);
    ZNode *znode = zset_query(entry->zset, score, name.data(), name.size());
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

uint32_t do_expire(std::vector<std::string> &cmd, std::string &out) {
    int64_t ttl_ms = 0;
    if (!str2int(cmd[2], ttl_ms)) {
        out_err(out, RES_ERR, "expect int64");
        return RES_ERR;
    }
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (node) {
        Entry *ent = container_of(node, Entry, node);
        entry_set_ttl(ent, ttl_ms);
    }
    out_int(out, node ? 1 : 0);
    return RES_OK;
}

uint32_t do_ttl(std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);
    if (!node) {
        out_int(out, -2);
        return RES_OK;
    }
    Entry *ent = container_of(node, Entry, node);
    if (ent->heap_idx == (size_t)-1) {
        out_int(out, -1);
        return RES_OK;
    }
    uint64_t expire_at = g_data.heap[ent->heap_idx].val;
    uint64_t now_us = get_monotonic_usec();
    out_int(out, expire_at > now_us ? (expire_at - now_us) / 1000 : 0);
    return RES_OK;
}

bool str2int(const std::string &s, int64_t &out) {
    char *end = nullptr;
    errno = 0;
    long long val = strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end != s.c_str() + s.size()) {
        return false;
    }
    out = val;
    return true;
}

// Threaded ZSet destructor
static void threaded_zset_destructor(void* arg) {
    Entry* ent = (Entry*)arg;
    if (ent->zset) {
        // Free all ZSet nodes and the ZSet itself
        // Free AVL tree nodes (if any)
        // Free hash map inside ZSet
        // For simplicity, just delete the ZSet (assuming its destructor is correct)
        delete ent->zset;
        ent->zset = nullptr;
    }
    delete ent;
}

void entry_del(Entry *ent) {
    if (ent->type == 1 && ent->zset) {
        // Offload ZSet deletion to thread pool
        thread_pool_queue(&g_data.tp, threaded_zset_destructor, ent);
    } else {
        delete ent;
    }
}