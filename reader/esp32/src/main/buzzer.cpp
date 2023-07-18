#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "rom/gpio.h"

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

#define PIN1    4
#define PIN2    5

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

static bool timer_isr_callback(gptimer_handle_t, const gptimer_alarm_event_data_t*, void*)
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

    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
        .flags = 0,
    };
    gptimer_handle_t gptimer = nullptr;
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &gptimer));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 125, // period = 125 microseconds @resolution 1MHz
        .reload_count = 0, // counter will reload with 0 on alarm event
        .flags = 0,
    };
    alarm_config.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, nullptr));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}
