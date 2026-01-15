#pragma once

#include <string>

void start_mqtt(const std::string& mqtt_address);

void log_mqtt(const std::string& msg);

void publish_mqtt_status(bool cameras_on,
                         bool estop_on);
