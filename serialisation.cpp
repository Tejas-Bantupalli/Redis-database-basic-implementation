#include <cstdio>        // for printf, perror
#include <cstdlib>       // for exit
#include <cstring>       // for memset, strlen
#include <unistd.h>      // for read, write, close
#include <sys/socket.h>  // for socket, setsockopt, bind, listen, accept
#include <netinet/in.h>  // for sockaddr_in, htons, htonl
#include <csignal>       // for signal (optional cleanup handling)
#include <cassert>       // for assert
#include <cstdint>       // for uint32_t
#include <arpa/inet.h>   // for inet_ntop (if needed)
#include <sys/types.h>  // for ssize_t
#include <vector>
#include <poll.h>        // for poll
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <sys/select.h>
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

void out_nil(std::string &out) {
    out.push_back(SER_NIL);
}

void out_str(std::string &out, const std::string &val) {
    out.push_back(SER_STR);
    uint32_t len = (uint32_t)val.size();
    out.append((char *)&len,4);
    out.append(val);
}

void out_int(std::string &out, int64_t val) {

    out.push_back(SER_INT);
    int64_t nval = htobe64(val);
    out.append((char *)&nval, 8);
    // Debug print
    printf("[DEBUG] out_int: val=%lld, nval=0x%016llx\n", (long long)val, (unsigned long long)nval);
    for (int i = 0; i < 8; ++i) {
        printf("%02x ", ((unsigned char*)&nval)[i]);
    }
    printf("\n");
}


void out_dbl(std::string &out, int64_t val) {

    out.push_back(SER_DOUBLE);
    int64_t nval = htobe64(val);
    out.append((char *)&nval, 8);
    // Debug print
    printf("[DEBUG] out_int: val=%lld, nval=0x%016llx\n", (long long)val, (unsigned long long)nval);
    for (int i = 0; i < 8; ++i) {
        printf("%02x ", ((unsigned char*)&nval)[i]);
    }
    printf("\n");
}
void out_err(std::string &out, int32_t code, const std::string &msg) {
    out.push_back(SER_ERR);
    out.append((char *)&code, 4);
    uint32_t len = (uint32_t)msg.size();
    out.append((char *)&len,4);
    out.append(msg);
}

void out_arr(std::string &out, uint32_t n) {
    out.push_back(SER_ARR);
    out.append((char *)&n, 4);
}
