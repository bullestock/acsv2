#pragma once

#include <string>

void start_mqtt(const std::string& mqtt_address);

void log_mqtt(const std::string& msg);

// /hal9k/acs/status/<ident>/<subtopic>
void set_mqtt_status(const std::string& subtopic,
                     const std::string& msg);
