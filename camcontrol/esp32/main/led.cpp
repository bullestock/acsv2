#include "led.h"
#include "gpio.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static bool steady = true;
static bool state = false;
static int flash_on = 0;
static int flash_off = 0;

void set_led_online_steady(bool on)
{
    steady = true;
    state = on;
}

void set_led_online_flash(int on_ms, int off_ms)
{
    steady = false;
    state = true;
    flash_on = on_ms;
    flash_off = off_ms;
}

extern "C"
void led_task(void*)
{
    uint64_t last_tick = 0;
    
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        set_led_online(state);
        if (!steady)
        {
            const auto now = esp_timer_get_time(); // microseconds
            const auto elapsed = (now - last_tick) / 1000;
            if (state)
            {
                if (elapsed >= flash_on)
                {
                    state = false;
                    last_tick = now;
                }
            }
            else
            {
                if (elapsed >= flash_off)
                {
                    state = true;
                    last_tick = now;
                }
            }
        }
    }
}
