#ifndef _LEDS_H_
#define _LEDS_H_

#include <stdint.h>

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;
} LED_RGB_T;

/// The two status LEDs, behind whichever driver the board's environment builds.
///
/// Two drivers exist side by side because the hardware is in transition: the prototype has one
/// four-pin common-cathode RGB LED per key on six PWM pins (leds_pwm.cpp, the default), while the
/// rev A board chains WS2812-compatible pixels on a single data pin (leds_pixel.cpp, selected with
/// LED_DRIVER_PIXEL). Everything above this interface, the modes and the mute/deafen gating, is
/// the same for both.

/// Configures the pins and leaves both LEDs dark.
void ledsBegin();

/// Shows the mute and deafen LEDs: each one in its colour scaled by its brightness when on, dark
/// when off. Callers rate-limit it; on the pixel driver every call sends a frame down the chain.
void ledsShow(const LED_RGB_T &mute, bool muteOn, const LED_RGB_T &deafen, bool deafenOn);

#endif
