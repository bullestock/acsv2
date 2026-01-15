#pragma once

#include <string>
#include <vector>

#include <driver/gpio.h>
#include <driver/uart.h>

constexpr const char* WIFI_KEY = "wifi";
constexpr const char* MQTT_KEY = "mqt";
constexpr const char* GATEWAY_TOKEN_KEY = "gwt";
constexpr const char* LOG_TOKEN_KEY = "lt";
constexpr const char* RELAY_KEY = "rl";

// Pin definitions
#ifdef CONFIG_IDF_TARGET_ESP32
constexpr const auto PIN_RELAY1 = (gpio_num_t) 17;
constexpr const auto PIN_RELAY2 = (gpio_num_t) 18;
constexpr const auto PIN_BUTTON = (gpio_num_t) 16;
constexpr const auto PIN_SDA = (gpio_num_t) 15;
constexpr const auto PIN_SCL = (gpio_num_t) 14;
#else
constexpr const auto PIN_RELAY1 = (gpio_num_t) 7;
constexpr const auto PIN_RELAY2 = (gpio_num_t) 8;
constexpr const auto PIN_BUTTON = (gpio_num_t) 6;
constexpr const auto PIN_SDA = (gpio_num_t) 5;
constexpr const auto PIN_SCL = (gpio_num_t) 4;
#endif

constexpr const auto DISPLAY_I2C_ADDRESS = 0x3C;

constexpr const char* TAG = "CAM";

using wifi_creds_t = std::vector<std::pair<std::string, std::string>>;
