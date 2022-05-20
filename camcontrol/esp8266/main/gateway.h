#pragma once

#include <atomic>

extern std::atomic<bool> relay_on;

extern "C" void gw_task(void*);
