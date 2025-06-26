#pragma once
#include <cstdint>
#include <cstdlib>

extern const size_t k_max_msg;
int32_t read_full(int fd, char* buf, size_t len);
int32_t write_all(int fd, const char* buf, size_t len);
void die(const char* msg);
