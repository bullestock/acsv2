#pragma once

#include "defs.h"

void init_nvs();

std::string get_gateway_token();
wifi_creds_t get_wifi_creds();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_gateway_token(const char* token);

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:

