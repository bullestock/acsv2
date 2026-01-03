#pragma once

#include "defs.h"

void init_nvs();

std::string get_mqtt_address();
std::string get_mqtt_password();
std::string get_acs_token();
std::string get_foreninglet_username();
std::string get_foreninglet_password();
std::string get_gateway_token();
std::string get_identifier();
std::string get_descriptor();
std::string get_slack_token();
wifi_creds_t get_wifi_creds();
bool get_beta_program_active();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_mqtt_address(const char* address);
void set_mqtt_password(const char* password);
void set_acs_token(const char* token);
void set_foreninglet_password(const char* pass);
void set_foreninglet_username(const char* user);
void set_gateway_token(const char* token);
void set_identifier(const char* identifier);
void set_descriptor(const char* descriptor);
void set_slack_token(const char* token);
void set_beta_program_active(bool active);
