#pragma once

#include <utility>

void init_gpio();

void set_relay(bool on);

std::pair<bool, bool> read_buttons();
