#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "../common.h"
#include "../hashtable.h"
#include "../zset.h"
#include "../utils.h"
#include "../serialisation.h"
#include <cmath>
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define be64toh(x) OSSwapBigToHostInt64(x)
#elif defined(_WIN32)
#error "Windows byte order macros not implemented"
#else
#include <endian.h>
#endif

void assert_dbl_response(const std::string& out, double expected) {
    assert(out.size() >= 9);
    assert((uint8_t)out[0] == 5); // SER_DOUBLE
    uint64_t nval = 0;
    memcpy(&nval, out.data() + 1, 8);
    nval = be64toh(nval);
    double val = 0;
    memcpy(&val, &nval, 8);
    assert(std::fabs(val - expected) < 1e-9);
}

void test_set_get_del_keys() {
    std::string out;
    std::vector<std::string> cmd;
    // set
    cmd = {"set", "foo", "bar"};
    assert(do_set(cmd, out) == RES_OK);
    // get
    out.clear();
    cmd = {"get", "foo"};
    assert(do_get(cmd, out) == RES_OK);
    assert(out.find("bar") != std::string::npos);
    // del
    out.clear();
    cmd = {"del", "foo"};
    assert(do_del(cmd, out) == RES_OK);
    // get non-existent
    out.clear();
    cmd = {"get", "foo"};
    assert(do_get(cmd, out) == RES_NX);
    // keys (should be empty)
    out.clear();
    cmd = {"keys"};
    assert(do_keys(cmd, out) == RES_OK);
}

void test_zset() {
    std::string out;
    std::vector<std::string> cmd;
    // zadd
    cmd = {"zadd", "myzset", "1.5", "alice"};
    assert(do_zadd(cmd, out) == RES_OK);
    // zscore
    out.clear();
    cmd = {"zscore", "myzset", "alice"};
    assert(do_zscore(cmd, out) == RES_OK);
    assert_dbl_response(out, 1.5);
    // zrem
    out.clear();
    cmd = {"zrem", "myzset", "alice"};
    assert(do_zrem(cmd, out) == RES_OK);
    // zscore non-existent
    out.clear();
    cmd = {"zscore", "myzset", "alice"};
    assert(do_zscore(cmd, out) == RES_NX);
    // zadd multiple
    out.clear();
    cmd = {"zadd", "myzset", "2.0", "bob"};
    assert(do_zadd(cmd, out) == RES_OK);
    cmd = {"zadd", "myzset", "3.0", "carol"};
    assert(do_zadd(cmd, out) == RES_OK);
    // zquery
    out.clear();
    cmd = {"zquery", "myzset", "2.0", "bob", "0", "2"};
    assert(do_query(cmd, out) == RES_OK);
    // Check that the response contains two entries: bob/2.0 and carol/3.0
    size_t pos = 0;
    for (int i = 0; i < 2; ++i) {
        assert(pos < out.size());
        assert((uint8_t)out[pos] == 2); // SER_STR
        uint32_t slen = 0;
        memcpy(&slen, out.data() + pos + 1, 4);
        std::string name = out.substr(pos + 5, slen);
        if (i == 0) assert(name == "bob");
        if (i == 1) assert(name == "carol");
        pos += 5 + slen;
        assert(pos < out.size());
        assert((uint8_t)out[pos] == 5); // SER_DOUBLE
        uint64_t nval = 0;
        memcpy(&nval, out.data() + pos + 1, 8);
        nval = be64toh(nval);
        double val = 0;
        memcpy(&val, &nval, 8);
        if (i == 0) assert(std::fabs(val - 2.0) < 1e-9);
        if (i == 1) assert(std::fabs(val - 3.0) < 1e-9);
        pos += 9;
    }
}

void test_edge_cases() {
    std::string out;
    std::vector<std::string> cmd;
    // set with empty key/value
    cmd = {"set", "", ""};
    assert(do_set(cmd, out) == RES_OK);
    // get empty key
    out.clear();
    cmd = {"get", ""};
    assert(do_get(cmd, out) == RES_OK);
    // del empty key
    out.clear();
    cmd = {"del", ""};
    assert(do_del(cmd, out) == RES_OK);
    // zadd with invalid score
    out.clear();
    cmd = {"zadd", "myzset", "notanumber", "dave"};
    try {
        do_zadd(cmd, out);
        assert(false && "Should throw on invalid score");
    } catch (...) {
        // expected
    }
    // zquery with invalid offset/limit
    out.clear();
    cmd = {"zquery", "myzset", "2.0", "bob", "notanumber", "notanumber"};
    try {
        do_query(cmd, out);
        assert(false && "Should throw on invalid offset/limit");
    } catch (...) {
        // expected
    }
}

int main() {
    test_set_get_del_keys();
    test_zset();
    test_edge_cases();
    std::cout << "All tests passed!\n";
    return 0;
} 