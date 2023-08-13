#pragma once

#include <stdint.h>

void init_rs485();

int read_rs485(char* buf, size_t buf_size);

void write_rs485(const char* data, size_t size);
