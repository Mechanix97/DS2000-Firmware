// Pixel driver: WS2812-compatible pixels (WS2812B, SK6812) chained on LED_DATA, mute first, then
// deafen. Selected with LED_DRIVER_PIXEL. Adafruit_NeoPixel drives the chain from a PIO state
// machine on the RP2040 and RP2350, so the CPU does not bit-bang the 800 kHz timing.
#ifdef LED_DRIVER_PIXEL

#include <Adafruit_NeoPixel.h>

#include "leds.h"
#include "pins.h"

static Adafruit_NeoPixel pixels(LED_COUNT, LED_DATA, LED_PIXEL_ORDER + NEO_KHZ800);

static uint32_t colourOf(const LED_RGB_T &led, bool on)
{
    if (!on)
    {
        return 0;
    }
    return Adafruit_NeoPixel::Color(led.red * led.brightness / 255, led.green * led.brightness / 255,
                                    led.blue * led.brightness / 255);
}

void ledsBegin()
{
    pixels.begin();
    pixels.clear();
    pixels.show();
}

void ledsShow(const LED_RGB_T &mute, bool muteOn, const LED_RGB_T &deafen, bool deafenOn)
{
    // A chain shorter than two, like the RP2350-Zero's single onboard pixel, shows the mute LED
    // only; setPixelColor ignores an index past the end.
    pixels.setPixelColor(0, colourOf(mute, muteOn));
    pixels.setPixelColor(1, colourOf(deafen, deafenOn));
    pixels.show();
}

#endif
