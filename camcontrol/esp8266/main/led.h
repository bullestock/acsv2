#pragma once

#include <NeoPixelBus.h>

class Led
{
public:
    Led();

    set_color(RgbColor col);

private:
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip;
};
