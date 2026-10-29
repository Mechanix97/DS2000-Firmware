#include <Arduino.h>

#include "leds.h"
#include "pins.h"
#include "debounce.h"
#include "messages.h"
#include "protocol.h"

// The message codes, button ids and RGB modes, and which bodies are accepted, live in
// lib/messages so the native tests can check them.
using namespace messages;

/// Wire format revision this firmware speaks, reported in MSG_DEVICE_INFO.
///
/// The application refuses a device whose revision differs from its own, so bump this with every
/// change to the framing, a message code or a payload layout, together with the application's
/// `PROTOCOL_VERSION`. Revision 1 introduced COBS framing with a CRC and the hello / device info
/// exchange; before it frames ended on a bare 0xFF and the handshake was an empty ping/pong.
const uint8_t PROTOCOL_VERSION = 1;

// Injected by scripts/build_info.py from platformio.ini.
#if !defined(FIRMWARE_VERSION_MAJOR) || !defined(FIRMWARE_VERSION_MINOR) || !defined(FIRMWARE_VERSION_PATCH)
#error "The firmware version is missing: build through PlatformIO so scripts/build_info.py runs"
#endif
#ifndef BOARD_ID
#error "BOARD_ID is missing: set custom_board_id for this environment in platformio.ini"
#endif

LED_RGB_T muteLed = {255, 255, 255, 255};
LED_RGB_T deafLed = {255, 255, 255, 255};

bool mute = false;
bool deafen = false;

/// One debouncer per key, fed from loop().
///
/// The keys used to be read by FALLING-edge interrupts with a 250 ms lockout, which turned the
/// release of a key held longer than that into a second press (#11). They are polled instead: a
/// pass of loop() takes microseconds, far below the 10 ms a press has to hold to count, and
/// polling keeps all key handling, Serial included, out of interrupt context.
debounce::Key muteKey;
debounce::Key deafenKey;
debounce::Key disconnectKey;

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

/// How long loop() may go without coming round before the hardware watchdog resets the chip.
///
/// Without it, a hang (a bug, or USB stuck in a state the core never returns from) left the device
/// dead until it was unplugged; with it, the device restarts and the application reconnects on its
/// own. A pass of loop() normally takes microseconds. The longest legitimate one is a Serial.write
/// to a host that has the port open but stopped reading, which the core gives up on after a
/// second, so 3 s leaves room for that without resetting a device that is only waiting.
const uint32_t WATCHDOG_TIMEOUT_MS = 3000;

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
    ledsShow(first, mute || deafen, second, deafen);
}

/// Sends one message body, framed: CRC, COBS and the delimiter are added here.
///
/// A frame goes out whole or not at all. When the host has the port open but has stopped reading,
/// the USB buffer fills up, and Serial.write waits for room for up to a second, with the keys and
/// LEDs frozen, and then gives up partway. The half frame it leaves on the wire has no delimiter,
/// so the host also loses the next frame, which it reads as the rest of this one and drops on
/// the CRC. A frame the host is not reading would be lost anyway, so it is dropped up front. A
/// device info dropped this way fails the application's handshake, which it retries on its
/// reconnect backoff.
void sendFrame(const uint8_t *body, size_t length)
{
    uint8_t wire[protocol::MAX_ENCODED_FRAME];
    const size_t wireLength = protocol::encodeFrame(body, length, wire);
    if (wireLength == 0 || Serial.availableForWrite() < (int)wireLength)
    {
        return;
    }
    Serial.write(wire, wireLength);
    Serial.flush();
}

/// Answers a hello with `[protocol][board][major][minor][patch]`, so the application can tell
/// which firmware it is talking to, refuse one whose protocol it does not speak, and pick the
/// right image when updating it.
void sendDeviceInfo()
{
    const uint8_t payload[] = {
        MSG_DEVICE_INFO,
        PROTOCOL_VERSION,
        BOARD_ID,
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

void handleVoiceSettings(bool newMute, bool newDeafen)
{
    if (newMute != mute || newDeafen != deafen)
    {
        mute = newMute;
        deafen = newDeafen;
        ledsDirty = true;
    }
}

/// Applies an RGB message that has already passed every check, so it lands whole.
void handleRgb(const Message &message)
{
    muteLed.brightness = message.brightness;
    deafLed.brightness = message.brightness;
    rgbMode = message.mode;
    rgbSpeed = message.speed;

    if (message.hasColours)
    {
        muteLed.red = message.muteColour.red;
        muteLed.green = message.muteColour.green;
        muteLed.blue = message.muteColour.blue;
        deafLed.red = message.deafenColour.red;
        deafLed.green = message.deafenColour.green;
        deafLed.blue = message.deafenColour.blue;
    }

    ledsDirty = true;
}

/// Acts on one decoded body. Which bodies are accepted, and which are dropped, is decided by
/// messages::parse.
void dispatch(const uint8_t *body, size_t length)
{
    const Message message = parse(body, length);
    switch (message.command)
    {
    case Command::HELLO:
        sendDeviceInfo();
        break;
    case Command::VOICE_SETTINGS:
        handleVoiceSettings(message.mute, message.deafen);
        break;
    case Command::RGB:
        handleRgb(message);
        break;
    case Command::REBOOT_TO_BOOTLOADER:
        // Nothing is answered: the port disappearing is the reply. The LEDs go dark so the device
        // does not look stuck on its last colour while it waits for an image.
        ledsShow(muteLed, false, deafLed, false);
        rp2040.rebootToBootloader();
        break;
    case Command::NONE:
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

/// Reports settled presses, and toggles locally so the LED follows the button even with no
/// application connected. The app confirms or corrects it by sending back a voice settings
/// message. The keys are active-low: pressed pulls the pin to ground.
void handle_buttons()
{
    const unsigned long now = millis();

    if (muteKey.update(digitalRead(MUTE_BUTTON) == LOW, now))
    {
        mute = !mute;
        ledsDirty = true;
        sendButton(BUTTON_MUTE);
    }

    if (deafenKey.update(digitalRead(DEAF_BUTTON) == LOW, now))
    {
        deafen = !deafen;
        ledsDirty = true;
        sendButton(BUTTON_DEAFEN);
    }

    if (disconnectKey.update(digitalRead(DISCONNECT_BUTTON) == LOW, now))
    {
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
    ledsBegin();

    Serial.begin(115200);

    // Started last, so nothing above counts against it.
    rp2040.wdt_begin(WATCHDOG_TIMEOUT_MS);
}

void loop()
{
    rp2040.wdt_reset();
    handle_serial_input();
    handle_buttons();
    set_led_pwm();
}
