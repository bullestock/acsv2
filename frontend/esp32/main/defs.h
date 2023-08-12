#pragma once

#include <driver/gpio.h>

constexpr const char* VERSION = "0.1";

// Pin definitions
constexpr const auto PIN_DISP_DC = (gpio_num_t) 2;
constexpr const auto PIN_DISP_RESET = (gpio_num_t) 4;
constexpr const auto PIN_DISP_CS = (gpio_num_t) 5;
constexpr const auto PIN_DISP_SCK = (gpio_num_t) 18;
constexpr const auto PIN_DISP_SDO = (gpio_num_t) 19;
constexpr const auto PIN_DISP_SDI = (gpio_num_t) 23;
constexpr const auto PIN_EXT_1 = (gpio_num_t) 27;
constexpr const auto PIN_EXT_2 = (gpio_num_t) 26;
constexpr const auto PIN_RELAY = (gpio_num_t) 21;

constexpr const char* TAG = "ACS";
