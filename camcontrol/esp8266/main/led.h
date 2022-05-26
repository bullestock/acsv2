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

enum Pattern {
    Initial,
    Black,
    BlueFlash,
    RedFlash,
    SolidRed,
    SolidGreen,
    GreenBlink,
    RedBlink,
    BlueBlink,
    GreenBlue,
};

void set_led_pattern(Pattern pat);

extern "C" void led_task(void*);
