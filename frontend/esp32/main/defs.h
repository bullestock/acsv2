#pragma once

#include <string>
#include <vector>

#include <driver/gpio.h>

constexpr const char* VERSION = "1.3.3";

constexpr const char* IDENTIFIER_KEY = "id";
constexpr const char* WIFI_KEY = "wifi";
constexpr const char* ACS_TOKEN_KEY = "act";
constexpr const char* GATEWAY_TOKEN_KEY = "gwt";
constexpr const char* SLACK_TOKEN_KEY = "slt";
constexpr const char* FL_USER_KEY = "flu";
constexpr const char* FL_PASS_KEY = "flp";

constexpr const int RS485_UART_PORT = 2;
constexpr const int RS485_BAUD_RATE = 9600;

// Pin definitions
constexpr const int RS485_TXD = 16;
constexpr const int RS485_RXD = 15;
constexpr const int RS485_RTS = 13;
constexpr const auto PIN_DISP_DC = (gpio_num_t) 2;
constexpr const auto PIN_DISP_RESET = (gpio_num_t) 4;
constexpr const auto PIN_DISP_CS = (gpio_num_t) 5;
constexpr const auto PIN_DISP_SCK = (gpio_num_t) 18;
constexpr const auto PIN_DISP_SDO = (gpio_num_t) 19;
constexpr const auto PIN_DISP_SDI = (gpio_num_t) 23;
constexpr const auto PIN_EXT_1 = (gpio_num_t) 27;
constexpr const auto PIN_EXT_2 = (gpio_num_t) 26;
constexpr const auto PIN_RELAY = (gpio_num_t) 21;
constexpr const auto PIN_RED = (gpio_num_t) 34;
constexpr const auto PIN_GREEN = (gpio_num_t) 32;
constexpr const auto PIN_WHITE = (gpio_num_t) 35;
constexpr const auto PIN_LEAVE = (gpio_num_t) 33;

constexpr const char* TAG = "ACS";

#define CARD_ID_FORMAT "%010llX"

using wifi_creds_t = std::vector<std::pair<std::string, std::string>>;
