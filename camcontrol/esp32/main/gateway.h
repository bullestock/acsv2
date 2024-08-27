#pragma once

#include <mutex>

#include <cJSON.h>

extern bool camera_relay_on;
extern bool estop_relay_on;
extern std::mutex relay_mutex;

extern "C" void gw_task(void*);
