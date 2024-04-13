#pragma once

#include "RDM6300.h"

#include <driver/uart.h>

#include <string>

#define VERSION "0.6"

constexpr auto CONSOLE_UART_PORT = (uart_port_t) 1;

constexpr const auto RS485_UART_PORT = (uart_port_t) 2;
constexpr const int RS485_BAUD_RATE = 9600;
constexpr const int RS485_TXD = 23;
constexpr const int RS485_RXD = 22;
constexpr const int RS485_RTS = 21;

RDM6300::Card_id get_and_clear_last_cardid();
