#pragma once

#include <utility>

void init_gpio();

void set_relay(bool on);

void set_led_online(bool on);

void set_led_camera(bool on);

bool read_button();
