#pragma once

#include <string>
#include <vector>

#include <driver/gpio.h>
#include <driver/uart.h>

constexpr const char* WIFI_KEY = "wifi";
constexpr const char* GATEWAY_TOKEN_KEY = "gwt";
constexpr const char* SLACK_TOKEN_KEY = "slt";
constexpr const char* RELAY1_KEY = "rl1";
constexpr const char* RELAY2_KEY = "rl2";

// Pin definitions
constexpr const auto PIN_DISP_DC = (gpio_num_t) 2;
constexpr const auto PIN_DISP_RESET = (gpio_num_t) 4;
constexpr const auto PIN_DISP_CS = (gpio_num_t) 5;
constexpr const auto PIN_DISP_SCK = (gpio_num_t) 18;
constexpr const auto PIN_DISP_SDO = (gpio_num_t) 19;
constexpr const auto PIN_DISP_SDI = (gpio_num_t) 23;
constexpr const auto PIN_RELAY1 = (gpio_num_t) 21;
constexpr const auto PIN_RELAY2 = (gpio_num_t) 21;
constexpr const auto PIN_BUTTON = (gpio_num_t) 22;

constexpr const auto DISPLAY_I2C_ADDRESS = 0x3C;

constexpr const char* TAG = "CAM";

using wifi_creds_t = std::vector<std::pair<std::string, std::string>>;
