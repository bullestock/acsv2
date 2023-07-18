#pragma once

#include <stdint.h>

void initialize_rs485();

int read_rs485(uint8_t* buf, size_t buf_size);

void write_rs485(const uint8_t* data, size_t size);
