// Default driver: one four-pin common-cathode RGB LED per key, each colour on its own PWM pin.
#ifndef LED_DRIVER_PIXEL

#include <Arduino.h>

#include "leds.h"
#include "pins.h"

static void writeLed(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, const LED_RGB_T &led, bool on)
{
    if (!on)
    {
        analogWrite(redPin, 0);
        analogWrite(greenPin, 0);
        analogWrite(bluePin, 0);
        return;
    }

    analogWrite(redPin, led.red * led.brightness / 255);
    analogWrite(greenPin, led.green * led.brightness / 255);
    analogWrite(bluePin, led.blue * led.brightness / 255);
}

void ledsBegin()
{
    pinMode(MUTE_LED_RED, OUTPUT);
    pinMode(MUTE_LED_GREEN, OUTPUT);
    pinMode(MUTE_LED_BLUE, OUTPUT);
    pinMode(DEAF_LED_RED, OUTPUT);
    pinMode(DEAF_LED_GREEN, OUTPUT);
    pinMode(DEAF_LED_BLUE, OUTPUT);
}

void ledsShow(const LED_RGB_T &mute, bool muteOn, const LED_RGB_T &deafen, bool deafenOn)
{
    writeLed(MUTE_LED_RED, MUTE_LED_GREEN, MUTE_LED_BLUE, mute, muteOn);
    writeLed(DEAF_LED_RED, DEAF_LED_GREEN, DEAF_LED_BLUE, deafen, deafenOn);
}

#endif
