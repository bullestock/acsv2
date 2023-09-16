#pragma once

#include "defs.h"

void init_nvs();

std::string get_gateway_token();
std::string get_slack_token();
wifi_creds_t get_wifi_creds();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_gateway_token(const char* token);
void set_slack_token(const char* token);

