#include "debounce.h"

namespace debounce
{
    bool Key::update(bool pressed, unsigned long now)
    {
        if (pressed != reading)
        {
            reading = pressed;
            readingSince = now;
        }

        // Unsigned subtraction, so the comparison survives millis() wrapping around.
        if (reading != settled && now - readingSince >= SETTLE_MS)
        {
            settled = reading;
            return settled;
        }
        return false;
    }
}
