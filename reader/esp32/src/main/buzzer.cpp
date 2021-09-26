#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/timer.h"

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

#define PIN1    19
#define PIN2    23

int beep_duration = 0;
int beep_interval = 0;
int beep_duration_left = 0;
int beep_interval_left = 0;
int beep_last_tick = 0;

void beep(int freq, int duration)
{
    beep_interval = 4000/freq;
    beep_interval_left = beep_interval;
    beep_duration = duration*8;
    beep_duration_left = beep_duration;
}

static bool IRAM_ATTR timer_isr_callback(void* args)
{
    static bool on = true;
    if (beep_duration_left >= 0)
    {
        --beep_duration_left;
        --beep_interval_left;
        if (beep_interval_left <= 0)
        {
            beep_interval_left = beep_interval;
            if (on)
            {
                REG_WRITE(GPIO_OUT_W1TS_REG, 1ULL << PIN1);
                REG_WRITE(GPIO_OUT_W1TC_REG, 1ULL << PIN2);
            }
            else
            {
                REG_WRITE(GPIO_OUT_W1TS_REG, 1ULL << PIN2);
                REG_WRITE(GPIO_OUT_W1TC_REG, 1ULL << PIN1);
            }
            on = !on;
        }
    }

    return false;
}

void init_buzzer()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN1) | (1ULL << PIN2);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = TIMER_DIVIDER,
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_SCALE/8000);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_isr_callback, nullptr, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);
}
