#pragma once

#include "defs.h"

void init_nvs();

std::string get_mqtt_address();
std::string get_gateway_token();
wifi_creds_t get_wifi_creds();
bool get_relay_state();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_mqtt_address(const char* address);
void set_gateway_token(const char* token);
void set_relay_state(bool);

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:

