#pragma once

#include "defs.h"

void init_nvs();

std::string get_acs_token();
std::string get_gateway_token();
std::string get_slack_token();
wifi_creds_t get_wifi_creds();
std::string get_foreninglet_username();
std::string get_foreninglet_password();
    
void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_acs_token(const char* token);
void set_gateway_token(const char* token);
void set_slack_token(const char* token);
void set_foreninglet_username(const char* user);
void set_foreninglet_password(const char* pass);
