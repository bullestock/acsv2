#pragma once

void set_led_online_steady(bool on);

void set_led_online_flash(int on_ms, int off_ms);

extern "C" void led_task(void*);
