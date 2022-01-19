#pragma once

#include <string>

constexpr const char* VERSION = "0.3";

constexpr const char* WIFI_KEY = "wifi";
constexpr const char* WIFI_ON_KEY = "wifion";

std::string get_and_clear_last_cardid();

extern int8_t wifi_active;
