#include "led.h"

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
