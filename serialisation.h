#pragma once
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
enum {
    SER_NIL = 0, // Like `NULL`
    SER_ERR = 1, // An error code and message
    SER_STR = 2, // A string
    SER_INT = 3, // A int64
    SER_ARR = 4, // Array
    SER_DOUBLE = 5,
};

void out_nil(std::string &out);
void out_str(std::string &out, const std::string &val);
void out_int(std::string &out, int64_t val);
void out_err(std::string &out, int32_t code, const std::string &msg);
void out_arr(std::string &out, uint32_t n);
void out_dbl(std::string &out, double val);