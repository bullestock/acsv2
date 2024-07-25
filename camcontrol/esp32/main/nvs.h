#pragma once

#include "defs.h"

void init_nvs();

std::string get_gateway_token();
std::string get_slack_token();
wifi_creds_t get_wifi_creds();
bool get_relay1_state();
bool get_relay2_state();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_gateway_token(const char* token);
void set_relay1_state(bool on);
void set_relay2_state(bool on);
