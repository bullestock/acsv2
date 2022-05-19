#pragma once

#include <NeoPixelBus.h>

class Led
{
public:
    Led();

    void set_color(RgbColor col);

private:
    NeoPixelBus<NeoRgbFeature, NeoEsp8266DmaSk6812Method> strip;
};

extern Led the_led;
