#pragma once

#include <NeoPixelBus.h>

class Led
{
public:
    Led();

    void set_color(RgbColor col);

private:
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip;
};

extern Led the_led;
