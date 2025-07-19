#pragma once
struct Conn;
#include "common.h"
#include <vector>
#include <poll.h>        // for poll
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <sys/select.h>

void fd_set_nb(int fd);
bool try_flush_buffer(Conn *conn);
void stateres(Conn *conn);
void statereq(Conn *conn);
bool one_request(Conn *conn);
bool try_fill_buffer(Conn *conn);
void connection_io(Conn *conn);
uint64_t str_hash(const uint8_t* data, size_t len);
void conn_put(std::vector<Conn *> &fd2conn, Conn *conn);
