#include "buttons.h"

#include "defs.h"

Buttons::Keys Buttons::read()
{
    return {
        !gpio_get_level(PIN_RED),
        !gpio_get_level(PIN_WHITE),
        !gpio_get_level(PIN_GREEN),
        !gpio_get_level(PIN_LEAVE)
    };
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
