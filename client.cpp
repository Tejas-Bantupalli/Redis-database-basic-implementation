#include <cstdio>        // for printf, perror
#include <cstdlib>       // for exit
#include <cstring>       // for memset, strlen
#include <unistd.h>      // for read, write, close
#include <sys/socket.h>  // for socket, connect
#include <netinet/in.h>  // for sockaddr_in, htons, htonl
#include <arpa/inet.h>   // for inet_ntop (if needed)
#include <sys/types.h>  // for ssize_t
#include <cstdint>       // for uint32_t
#include <cassert>       // for assert
#include "common.h"
#include "serialisation.h"
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#elif defined(_WIN32)
#error "Windows byte order macros not implemented"
#else
#include <endian.h>
#endif
static int32_t on_response(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    switch (data[0])
    {
    case SER_NIL:
        printf("(nil)\n");
        return 1;
    case SER_ERR: {
        if (size < 9) {
            return -1;
        }
        int32_t code = 0;
        uint32_t len = 0;
        memcpy(&code, &data[1], 4);
        memcpy(&len, &data[5], 4);
        if (size < 9 + len) {
            return -1;
        }
        printf("(err) %d %.*s\n", code, len, &data[9]);
        return 9 + len;
    }
    case SER_ARR: {
        if (size < 5) return -1;
        uint32_t len = 0;
        memcpy(&len, &data[1], 4);
        printf("(arr) len=%u\n", len);
        size_t arr_bytes = 5;
        for (uint32_t i = 0; i < len; i++) {
            int32_t rv = on_response(&data[arr_bytes], size - arr_bytes);
            if (rv < 0) {
                return rv;
            }
            arr_bytes += (size_t)rv;
        }
        printf("(arr) end\n");
        return (int32_t)arr_bytes;
    }
    case SER_INT: {
        if (size < 9) return -1;
        int64_t integer = 0;
        printf("[DEBUG] SER_INT raw bytes: ");
        for (int i = 1; i < 9; ++i) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        memcpy(&integer, &data[1], 8);
        int64_t orig_integer = integer;
        integer = be64toh(integer);
        printf("int: %lld (raw: 0x%016llx)\n", (long long)integer, (unsigned long long)orig_integer);
        return 9;
    }
    case SER_STR: {
        if (size < 5) return -1;
        uint32_t len = 0;
        memcpy(&len, &data[1], 4);
        if (size < 5 + len) return -1;
        std::string out(reinterpret_cast<const char*>(&data[5]), len);
        printf("string: %s\n", out.c_str());
        return 5 + len;
    }
    default:
        printf("(unknown type)\n");
        return -1;
    }
}
static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;
    for (const std::string &arg : cmd) {
        len += 4 + (uint32_t)arg.size();
        if (len > k_max_msg) {
            return -1;
        }
    }
    if (len > k_max_msg) {
        return -1;
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4); // total length
    uint32_t n = cmd.size();
    memcpy(&wbuf[4], &n, 4); // number of arguments
    size_t cur = 8;
    for (const std::string &arg : cmd) {
        uint32_t arglen = (uint32_t)arg.size();
        if (cur + 4 + arglen > sizeof(wbuf)) {
            return -1;
        }
        memcpy(&wbuf[cur], &arglen, 4);
        memcpy(&wbuf[cur + 4], arg.data(), arglen);
        cur += 4 + arglen;
    }
    int32_t err = write_all(fd, wbuf, 4 + len);
    return err;
}

static int32_t read_resp(int fd) {
    char rbuf[4 + k_max_msg + 1]; // +1 for null-termination
    uint32_t len = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err == -1) {
        return -1; // Indicate an error
    }
    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        return -1; // Indicate an error
    }
    err = read_full(fd, &rbuf[4], len);
    if (err == -1) {
        return -1; // Indicate an error
    }
    if (len < 1) {
        printf("bad response\n");
        return -1;
    }
    uint8_t *data = reinterpret_cast<uint8_t*>(&rbuf[4]);
    return on_response(data, len);
}











static int32_t query(int fd, const char *text){
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1; // Indicate an error
    }
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    int32_t err = write_all(fd, wbuf, 4 + len);
    if (err == -1) {
        return -1; // Indicate an error
    }
    return read_resp(fd); // Read the response
}

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1 (localhost)

    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        close(fd);
        return 1;
    }
    err = read_resp(fd);
    if (err) {
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}