#include <Arduino.h>

#include "pins.h"
#include "protocol.h"

/// Ignore further edges from the same button for this long after one is accepted.
const unsigned long DEBOUNCE_TIMEOUT = 250;

/// Message codes shared with the desktop application. Changing one breaks every device already
/// flashed, so they are part of the contract rather than an implementation detail.
enum MessageCode : uint8_t
{
    /// The application opening a session. Answered with MSG_DEVICE_INFO.
    MSG_HELLO = 0x00,
    /// `[protocol][major][minor][patch]`: what the application decides compatibility on.
    MSG_DEVICE_INFO = 0x01,
    MSG_BUTTON = 0x02,
    MSG_VOICE_SETTINGS = 0x03,
    MSG_RGB = 0x04,
};

enum ButtonId : uint8_t
{
    BUTTON_MUTE = 0x00,
    BUTTON_DEAFEN = 0x01,
    BUTTON_DISCONNECT = 0x02,
};

/// Wire format revision this firmware speaks, reported in MSG_DEVICE_INFO.
///
/// The application refuses a device whose revision differs from its own, so bump this with every
/// change to the framing, a message code or a payload layout, together with the application's
/// `PROTOCOL_VERSION`. Revision 1 introduced COBS framing with a CRC and the hello / device info
/// exchange; before it frames ended on a bare 0xFF and the handshake was an empty ping/pong.
const uint8_t PROTOCOL_VERSION = 1;

// Injected by scripts/version.py from platformio.ini.
#if !defined(FIRMWARE_VERSION_MAJOR) || !defined(FIRMWARE_VERSION_MINOR) || !defined(FIRMWARE_VERSION_PATCH)
#error "The firmware version is missing: build through PlatformIO so scripts/version.py runs"
#endif

enum RgbMode : uint8_t
{
    RGB_MODE_RAINBOW = 0x00,
    RGB_MODE_FIXED = 0x01,
    RGB_MODE_BREATHING = 0x02,
};

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;
} LED_RGB_T;

LED_RGB_T muteLed = {255, 255, 255, 255};
LED_RGB_T deafLed = {255, 255, 255, 255};

volatile bool mute = false;
volatile bool deafen = false;

/// Set by the interrupt handlers, acted on in loop().
///
/// The handlers used to write to Serial directly, but Serial.flush() blocks until the USB buffer
/// drains and doing that inside an interrupt can stall the core or lose the write entirely. An
/// interrupt should record what happened and return.
volatile bool muteButtonPressed = false;
volatile bool deafenButtonPressed = false;
volatile bool disconnectButtonPressed = false;

/// One timestamp per button. A single shared one meant pressing mute and then deafen within the
/// debounce window silently dropped the second press.
volatile unsigned long lastMuteInterrupt = 0;
volatile unsigned long lastDeafenInterrupt = 0;
volatile unsigned long lastDisconnectInterrupt = 0;

/// Whether the LEDs need rewriting. Without it the PWM registers were rewritten on every pass of
/// loop(), thousands of times a second, to keep showing the same colour.
bool ledsDirty = true;

/// The mode the application last asked for.
///
/// This used to be a local inside handleRgb, read only to decide whether six colour bytes
/// followed. Nothing remembered it, so the animated modes had nowhere to live: the rainbow froze
/// on the last colour it happened to be given and breathing was indistinguishable from fixed.
uint8_t rgbMode = RGB_MODE_FIXED;

/// When the last animation frame was drawn, so the animated modes advance on wall-clock time
/// rather than on however fast loop() happens to spin.
unsigned long lastAnimationFrame = 0;

/// 60 Hz. Smooth to the eye, and still leaves the loop overwhelmingly idle.
const unsigned long ANIMATION_INTERVAL = 16;

/// How fast the animated modes run, as the application last set it. Higher is faster.
///
/// 128 is the midpoint and reproduces the periods this firmware used before the control existed,
/// so a device that never hears from the application still behaves the way it always did.
uint8_t rgbSpeed = 128;

const uint8_t SPEED_MAX = 255;

/// Bounds for one full hue sweep and one full inhale-exhale, slowest to fastest.
///
/// Breathing runs to a shorter floor than the rainbow. A lap of the colour wheel stays legible
/// far quicker than a fade does, which starts reading as a flicker rather than a breath.
const unsigned long RAINBOW_PERIOD_SLOWEST = 12000;
const unsigned long RAINBOW_PERIOD_FASTEST = 1000;
const unsigned long BREATH_PERIOD_SLOWEST = 6000;
const unsigned long BREATH_PERIOD_FASTEST = 600;

void writeLed(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, const LED_RGB_T &led, bool on)
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

/// Full-saturation colour for a hue in 0..1535.
///
/// Six 256-wide sectors of the colour wheel, walked with integer arithmetic. A real HSV
/// conversion would need floating point for no visible gain: at 256 steps per sector the
/// staircase is already finer than the LED can resolve.
void hueToRgb(uint16_t hue, uint8_t &red, uint8_t &green, uint8_t &blue)
{
    const uint8_t sector = hue / 256;
    const uint8_t offset = hue % 256;

    switch (sector)
    {
    case 0:  red = 255;          green = offset;       blue = 0;            break;
    case 1:  red = 255 - offset; green = 255;          blue = 0;            break;
    case 2:  red = 0;            green = 255;          blue = offset;       break;
    case 3:  red = 0;            green = 255 - offset; blue = 255;          break;
    case 4:  red = offset;       green = 0;            blue = 255;          break;
    default: red = 255;          green = 0;            blue = 255 - offset; break;
    }
}

/// Turns the speed byte into a period, in milliseconds.
///
/// Inverted, because the control is a speed and the animation needs a duration: turning it up has
/// to shorten the lap, not stretch it.
unsigned long periodFor(unsigned long slowest, unsigned long fastest)
{
    return slowest - (unsigned long)rgbSpeed * (slowest - fastest) / SPEED_MAX;
}

/// Where in the breath we are, 0 (dark) to 255 (full).
///
/// The triangle is squared because perceived brightness is nowhere near linear in duty cycle: a
/// bare triangle reads as a hard bounce at the top and a long dead stretch at the bottom, which
/// does not look like breathing.
uint8_t breathLevel(unsigned long now)
{
    const unsigned long period = periodFor(BREATH_PERIOD_SLOWEST, BREATH_PERIOD_FASTEST);
    const uint16_t phase = (uint32_t)(now % period) * 512 / period;
    const uint8_t triangle = phase < 256 ? phase : 511 - phase;
    return (uint16_t)triangle * triangle / 255;
}

void set_led_pwm()
{
    const unsigned long now = millis();
    const bool animated = (rgbMode == RGB_MODE_RAINBOW || rgbMode == RGB_MODE_BREATHING);

    if (animated)
    {
        // Rate-limited rather than dirty-checked: an animation is never done changing, so it
        // drives the PWM on a clock of its own instead of waiting to be told something moved.
        if (now - lastAnimationFrame < ANIMATION_INTERVAL)
        {
            return;
        }
        lastAnimationFrame = now;
    }
    else if (!ledsDirty)
    {
        return;
    }
    ledsDirty = false;

    LED_RGB_T first = muteLed;
    LED_RGB_T second = deafLed;

    if (rgbMode == RGB_MODE_RAINBOW)
    {
        // The rainbow owns the colour, so whatever was last configured is ignored while it runs.
        // Both LEDs share a hue: with only two of them, offsetting them reads as a fault rather
        // than as an effect.
        const unsigned long period = periodFor(RAINBOW_PERIOD_SLOWEST, RAINBOW_PERIOD_FASTEST);
        const uint16_t hue = (uint32_t)(now % period) * 1536 / period;
        hueToRgb(hue, first.red, first.green, first.blue);
        second.red = first.red;
        second.green = first.green;
        second.blue = first.blue;
    }
    else if (rgbMode == RGB_MODE_BREATHING)
    {
        // Breathing keeps the configured colour and modulates only the brightness, on top of the
        // level the application asked for rather than replacing it.
        const uint8_t level = breathLevel(now);
        first.brightness = (uint16_t)first.brightness * level / 255;
        second.brightness = (uint16_t)second.brightness * level / 255;
    }

    // Deafening implies muting, which is what Discord itself enforces, so the mute LED follows
    // both.
    writeLed(MUTE_LED_RED, MUTE_LED_GREEN, MUTE_LED_BLUE, first, mute || deafen);
    writeLed(DEAF_LED_RED, DEAF_LED_GREEN, DEAF_LED_BLUE, second, deafen);
}

/// Sends one message body, framed: CRC, COBS and the delimiter are added here.
void sendFrame(const uint8_t *body, size_t length)
{
    uint8_t wire[protocol::MAX_ENCODED_FRAME];
    const size_t wireLength = protocol::encodeFrame(body, length, wire);
    if (wireLength == 0)
    {
        return;
    }
    Serial.write(wire, wireLength);
    Serial.flush();
}

/// Answers a hello with `[protocol][major][minor][patch]`, so the application can tell which
/// firmware it is talking to and refuse one whose protocol it does not speak.
void sendDeviceInfo()
{
    const uint8_t payload[] = {
        MSG_DEVICE_INFO,
        PROTOCOL_VERSION,
        FIRMWARE_VERSION_MAJOR,
        FIRMWARE_VERSION_MINOR,
        FIRMWARE_VERSION_PATCH,
    };
    sendFrame(payload, sizeof(payload));
}

void sendButton(uint8_t button)
{
    const uint8_t payload[] = {MSG_BUTTON, button};
    sendFrame(payload, sizeof(payload));
}

void handleVoiceSettings(uint8_t m, uint8_t d)
{
    bool newMute = (m != 0x00);
    bool newDeafen = (d != 0x00);

    if (newMute != mute || newDeafen != deafen)
    {
        mute = newMute;
        deafen = newDeafen;
        ledsDirty = true;
    }
}

void handleRgb(const uint8_t *payload, uint8_t length)
{
    // Brightness, mode and speed. The six colour bytes follow only for the modes that use them.
    if (length < 4)
    {
        return;
    }

    const uint8_t mode = payload[2];
    if (mode != RGB_MODE_RAINBOW && mode != RGB_MODE_FIXED && mode != RGB_MODE_BREATHING)
    {
        // Drop the whole frame rather than adopt half of it: a mode byte this side does not know
        // means the desktop application is ahead of this firmware, and guessing would leave the
        // brightness applied under an effect that was never asked for.
        return;
    }

    const bool carriesColours = (mode == RGB_MODE_FIXED || mode == RGB_MODE_BREATHING);
    if (carriesColours && length < 10)
    {
        return;
    }

    // Nothing is written until every check has passed. Validating as it went meant a truncated
    // frame still landed its brightness and mode before bailing out, leaving the device lit by
    // half a message it had already rejected.
    muteLed.brightness = payload[1];
    deafLed.brightness = payload[1];
    rgbMode = mode;
    rgbSpeed = payload[3];

    if (carriesColours)
    {
        muteLed.red = payload[4];
        muteLed.green = payload[5];
        muteLed.blue = payload[6];
        deafLed.red = payload[7];
        deafLed.green = payload[8];
        deafLed.blue = payload[9];
    }

    ledsDirty = true;
}

void dispatch(const uint8_t *payload, uint8_t length)
{
    switch (payload[0])
    {
    case MSG_HELLO:
        // Exactly the code: anything longer is not a hello, whatever its first byte says.
        if (length == 1)
        {
            sendDeviceInfo();
        }
        break;
    case MSG_VOICE_SETTINGS:
        if (length >= 3)
        {
            handleVoiceSettings(payload[1], payload[2]);
        }
        break;
    case MSG_RGB:
        handleRgb(payload, length);
        break;
    case MSG_DEVICE_INFO:
    case MSG_BUTTON:
        // Sent by this device, never received by it.
        break;
    default:
        break;
    }
}

/// Collects encoded bytes up to each delimiter, then decodes and dispatches the frame.
///
/// A frame whose COBS or CRC does not check out is dropped whole. Before the CRC, a frame missing
/// its first byte was simply read as whichever message the next byte named.
void handle_serial_input()
{
    static uint8_t buf[protocol::MAX_ENCODED_FRAME];
    static size_t i = 0;
    /// Set when a frame outgrew the buffer, so its tail is not mistaken for the start of another.
    static bool overflowed = false;

    while (Serial.available())
    {
        const uint8_t c = Serial.read();

        if (c == protocol::FRAME_DELIMITER)
        {
            size_t bodyLength = 0;
            // Back-to-back delimiters carry nothing and are skipped.
            if (!overflowed && i > 0 && protocol::decodeFrame(buf, i, bodyLength))
            {
                dispatch(buf, bodyLength);
            }
            i = 0;
            overflowed = false;
            continue;
        }

        if (i < sizeof(buf))
        {
            buf[i++] = c;
        }
        else
        {
            overflowed = true;
        }
    }
}

void muteButtonISR()
{
    unsigned long now = millis();
    if (now - lastMuteInterrupt > DEBOUNCE_TIMEOUT)
    {
        lastMuteInterrupt = now;
        muteButtonPressed = true;
    }
}

void deafenButtonISR()
{
    unsigned long now = millis();
    if (now - lastDeafenInterrupt > DEBOUNCE_TIMEOUT)
    {
        lastDeafenInterrupt = now;
        deafenButtonPressed = true;
    }
}

void disconnectButtonISR()
{
    unsigned long now = millis();
    if (now - lastDisconnectInterrupt > DEBOUNCE_TIMEOUT)
    {
        lastDisconnectInterrupt = now;
        disconnectButtonPressed = true;
    }
}

/// Reports the presses the interrupts recorded, and toggles locally so the LED follows the
/// button even with no application connected. The app confirms or corrects it by sending back a
/// voice settings message.
void handle_buttons()
{
    if (muteButtonPressed)
    {
        muteButtonPressed = false;
        mute = !mute;
        ledsDirty = true;
        sendButton(BUTTON_MUTE);
    }

    if (deafenButtonPressed)
    {
        deafenButtonPressed = false;
        deafen = !deafen;
        ledsDirty = true;
        sendButton(BUTTON_DEAFEN);
    }

    if (disconnectButtonPressed)
    {
        disconnectButtonPressed = false;
        sendButton(BUTTON_DISCONNECT);
    }
}

void setup()
{
    pinMode(MUTE_BUTTON, INPUT_PULLUP);
    pinMode(DEAF_BUTTON, INPUT_PULLUP);
    pinMode(DISCONNECT_BUTTON, INPUT_PULLUP);

    // No LED_BUILTIN here: it was configured but never written to, and the board this runs on
    // has a WS2812 on GPIO16 instead of a plain LED, so the constant does not even exist for it.
    pinMode(MUTE_LED_RED, OUTPUT);
    pinMode(MUTE_LED_GREEN, OUTPUT);
    pinMode(MUTE_LED_BLUE, OUTPUT);
    pinMode(DEAF_LED_RED, OUTPUT);
    pinMode(DEAF_LED_GREEN, OUTPUT);
    pinMode(DEAF_LED_BLUE, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(MUTE_BUTTON), muteButtonISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(DEAF_BUTTON), deafenButtonISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(DISCONNECT_BUTTON), disconnectButtonISR, FALLING);

    Serial.begin(115200);
}

void loop()
{
    handle_serial_input();
    handle_buttons();
    set_led_pwm();
}
