#include "buttons.h"

#include "defs.h"

Buttons::Keys Buttons::read()
{
#ifdef PLATFORM_DEVKIT
#pragma message("Building for devkit")
    // No buttons
    return { false, false, false, false };
#else
    return {
        !gpio_get_level(PIN_RED),
        !gpio_get_level(PIN_WHITE),
        !gpio_get_level(PIN_GREEN),
        !gpio_get_level(PIN_LEAVE)
    };
#endif
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
