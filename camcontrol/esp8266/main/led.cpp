#include "led.h"

#include <atomic>

Led::Led()
    : strip(1)
{
    strip.Begin();
    strip.Show();
}

void Led::set_color(RgbColor col)
{
    strip.SetPixelColor(0, col);
    strip.Show();
}

std::atomic<Pattern> pattern = Black;

void set_led_pattern(Pattern pat)
{
    pattern = pat;
}

Led the_led;

void led_task(void*)
{
    Pattern old_pattern = Initial;
    auto last_tick = 0;
    int state = 0;
    RgbColor flash_color;
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        const auto cur_tick = xTaskGetTickCount();
        const auto new_pattern = pattern.load();
        if (old_pattern != new_pattern)
            the_led.set_color(RgbColor(0, 0, 0));
        switch (new_pattern)
        {
        case Black:
            break;
            
        case BlueFlash:
        case RedFlash:
            flash_color = new_pattern == BlueFlash ? RgbColor(0, 0, 50) : RgbColor(50, 0, 0);
            switch (state)
            {
            case 0:
                if (cur_tick - last_tick >= 100/portTICK_PERIOD_MS)
                {
                    the_led.set_color(flash_color);
                    state = 1;
                    last_tick = cur_tick;
                }
                break;
            case 1:
                if (cur_tick - last_tick >= 1000/portTICK_PERIOD_MS)
                {
                    the_led.set_color(RgbColor(0, 0, 0));
                    state = 0;
                    last_tick = cur_tick;
                }
                break;
            default:
                state = 0;
            }
            break;
            
        case SolidRed:
        case SolidGreen:
            if (old_pattern != new_pattern)
            {
                the_led.set_color(new_pattern == SolidRed ? RgbColor(50, 0, 0) : RgbColor(0, 50, 0));
            }
            break;

        case GreenBlink:
        case RedBlink:
        case BlueBlink:
            switch (new_pattern)
            {
            case GreenBlink:
                flash_color = RgbColor(0, 50, 0);
                break;
            case RedBlink:
                flash_color = RgbColor(50, 0, 0);
                break;
            default:
                flash_color = RgbColor(0, 0, 50);
                break;
            }
            switch (state)
            {
            case 0:
                if (cur_tick - last_tick >= 5000/portTICK_PERIOD_MS)
                {
                    the_led.set_color(flash_color);
                    state = 1;
                    last_tick = cur_tick;
                }
                break;
            case 1:
                if (cur_tick - last_tick >= 50/portTICK_PERIOD_MS)
                {
                    the_led.set_color(RgbColor(0, 0, 0));
                    state = 0;
                    last_tick = cur_tick;
                }
                break;
            default:
                state = 0;
            }
            break;

        case GreenBlue:
            switch (state)
            {
            case 0:
                if (cur_tick - last_tick >= 5000/portTICK_PERIOD_MS)
                {
                    the_led.set_color(RgbColor(0, 0, 50));
                    state = 1;
                    last_tick = cur_tick;
                }
                break;
            case 1:
                if (cur_tick - last_tick >= 50/portTICK_PERIOD_MS)
                {
                    the_led.set_color(RgbColor(0, 50, 0));
                    state = 0;
                    last_tick = cur_tick;
                }
                break;
            default:
                state = 0;
            }
            break;

        default:
            break;
        }
        old_pattern = new_pattern;
    }
}


