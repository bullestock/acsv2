#include "led.h"

#include <ctype.h>
#include <stdio.h>

#include <mutex>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>

const auto PIN_RED = (gpio_num_t) 14;
const auto PIN_GREEN = (gpio_num_t) 13;

const int MAX_SEQ_SIZE = 250;
enum class Sequence
{
    Red,
    Green,
    Both,
    None
};

static std::mutex mutex;
char sequence[MAX_SEQ_SIZE];
int sequence_len = 0;
int sequence_index = 0;
int sequence_period = 1;
int sequence_repeats = 1; // to force idle sequence on startup
int sequence_iteration = 0;

int pwm_max = 64;

bool fill_seq(char* seq, int& index, int reps, Sequence elem)
{
    while (reps--)
    {
        if (index >= MAX_SEQ_SIZE)
        {
            printf("Sequence too long\n");
            return false;
        }
        seq[index++] = (char) elem;
    }
    return true;
}

void set_idle_pattern()
{
    // Idle LED pattern: P10R0SGX99N
    sequence_index = 0;
    sequence_period = 1;
    sequence_repeats = 0;
    sequence_iteration = 0;
    int index = 0;
    sequence[index++] = (char) Sequence::Green;
    fill_seq(sequence, index, 99, Sequence::None);
    sequence_len = index;
}

void update_leds()
{
    static int delay_counter = 0;
    std::lock_guard<std::mutex> g(mutex);
    if (++delay_counter < sequence_period)
        return;
    delay_counter = 0;
    if (sequence_index >= sequence_len)
    {
        sequence_index = 0;
        if (sequence_repeats > 0)
        {
            if (sequence_iteration >= sequence_repeats)
            {
                // Done
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
                sequence_len = 0;
                set_idle_pattern();
                return;
            }
            ++sequence_iteration;
        }
    }
    if (sequence_index < sequence_len)
    {
        switch ((Sequence) sequence[sequence_index++])
        {
        case Sequence::Red:
            ESP_ERROR_CHECK(gpio_set_level(PIN_GREEN, 0));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_max));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            break;
        case Sequence::Green:
            ESP_ERROR_CHECK(gpio_set_level(PIN_RED, 0));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, pwm_max));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
            break;
        case Sequence::Both:
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_max));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, pwm_max));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
            break;
        case Sequence::None:
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
            break;
        }
    }
}

extern "C"
void led_task(void*)
{
    // Configure GPIO pins
    
    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL << PIN_RED) | (1ULL << PIN_GREEN);
    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    // disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    // configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 1000,  // Set output frequency at 1 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = PIN_RED,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.gpio_num = PIN_GREEN;
    ledc_channel.channel = LEDC_CHANNEL_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    while (1)
    {
        update_leds();
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

bool parse_int(const char* line, int& index, int& value)
{
    if (!isdigit(line[index]))
    {
        printf("Expected number, got %d at %d\n", (int) line[index], index);
        return false;
    }
    value = 0;
    while (isdigit(line[index]))
    {
        value = value*10 + line[index] - '0';
        ++index;
    }
    return true;
}

bool parse_led_pattern(const char* line)
{
    // P<period>R<repeats>S<sequence>
    int period = 0;
    int i = 0;
    if (!parse_int(line, i, period))
    {
        printf("Period must follow P: %s\n", line);
        return false;
    }
    if (period <= 0)
    {
        printf("Period cannot be zero: %s\n", line);
        return false;
    }
    if (tolower(line[i]) != 'r')
    {
        printf("Period must be followed by R, got '%c' in '%s'\n", line[i], line);
        return false;
    }
    ++i;
    int repeats = 0;
    if (!parse_int(line, i, repeats))
    {
        printf("Repeats must follow R: %s\n", line);
        return false;
    }
    if (tolower(line[i]) != 's')
    {
        printf("Repeats must be followed by S, got '%c' in '%s'\n", line[i], line);
        return false;
    }
    ++i;
    char seq[MAX_SEQ_SIZE];
    int seq_len = 0;
    while (line[i])
    {
        if (seq_len == MAX_SEQ_SIZE)
        {
            printf("Sequence too long: %s\n", line);
            return false;
        }
        switch (tolower(line[i]))
        {
        case 'r':
            seq[seq_len++] = (char) Sequence::Red;
            break;
        case 'g':
            seq[seq_len++] = (char) Sequence::Green;
            break;
        case 'b':
            seq[seq_len++] = (char) Sequence::Both;
            break;
        case 'n':
            seq[seq_len++] = (char) Sequence::None;
            break;
        case 'x':
        {
            int reps = 0;
            ++i;
            if (!parse_int(line, i, reps))
            {
                printf("X must be followed by repeats: %s\n", line);
                return false;
            }
            switch (tolower(line[i]))
            {
            case 'r':
                if (!fill_seq(seq, seq_len, reps, Sequence::Red))
                    return false;
                break;
            case 'g':
                if (!fill_seq(seq, seq_len, reps, Sequence::Green))
                    return false;
                break;
            case 'b':
                if (!fill_seq(seq, seq_len, reps, Sequence::Both))
                    return false;
                break;
            case 'n':
                if (!fill_seq(seq, seq_len, reps, Sequence::None))
                    return false;
                break;
            default:
                printf("Unexpected character after X: '%c' in '%s'\n", line[i], line);
                return false;
            }
        }
        break;
        default:
            printf("Unexpected sequence character '%c' in '%s'\n", line[i], line);
            return false;
        }
        ++i;
    }
    printf("Period %d repeats %d\n", period, repeats);
    {
        std::lock_guard<std::mutex> g(mutex);
        sequence_index = 0;
        sequence_period = period/10;
        sequence_repeats = repeats;
        sequence_iteration = 0;
        for (int i = 0; i < seq_len; ++i)
            sequence[i] = seq[i];
        sequence_len = seq_len;
    }
    printf("OK\n");
    return true;
}
