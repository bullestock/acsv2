#pragma once

#include "defs.h"

void init_nvs();

std::string get_mqtt_address();
std::string get_acs_token();
std::string get_foreninglet_username();
std::string get_foreninglet_password();
std::string get_identifier();
std::string get_slack_token();
const uint8_t* get_private_key();
wifi_creds_t get_wifi_creds();
bool get_is_main();

void clear_wifi_credentials();
void add_wifi_credentials(const char* ssid, const char* password);
void set_mqtt_address(const char* address);
void set_acs_token(const char* token);
void set_foreninglet_password(const char* pass);
void set_foreninglet_username(const char* user);
void set_identifier(const char* identifier);
void set_slack_token(const char* token);
void set_private_key(const uint8_t* key);
void set_is_main(bool is_main);

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
