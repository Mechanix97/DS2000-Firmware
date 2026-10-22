#include "messages.h"

#include <string.h>

namespace messages
{
    static Message parseRgb(const uint8_t *body, size_t length)
    {
        Message message;

        // Brightness, mode and speed. The six colour bytes follow only for the modes that use them.
        if (length < 4)
        {
            return message;
        }

        const uint8_t mode = body[2];
        if (mode != RGB_MODE_RAINBOW && mode != RGB_MODE_FIXED && mode != RGB_MODE_BREATHING)
        {
            // Drop the whole frame rather than adopt half of it: a mode byte this side does not
            // know means the desktop application is ahead of this firmware, and guessing would
            // leave the brightness applied under an effect that was never asked for.
            return message;
        }

        const bool hasColours = (mode == RGB_MODE_FIXED || mode == RGB_MODE_BREATHING);
        if (hasColours && length < 10)
        {
            return message;
        }

        // Nothing is set until every check has passed, so a rejected frame changes nothing. When
        // this validated as it went, a truncated frame still landed its brightness and mode before
        // bailing out, leaving the device lit by half a message it had already rejected.
        message.command = Command::RGB;
        message.brightness = body[1];
        message.mode = mode;
        message.speed = body[3];
        message.hasColours = hasColours;
        if (hasColours)
        {
            message.muteColour = {body[4], body[5], body[6]};
            message.deafenColour = {body[7], body[8], body[9]};
        }
        return message;
    }

    Message parse(const uint8_t *body, size_t length)
    {
        Message message;
        if (length == 0)
        {
            return message;
        }

        switch (body[0])
        {
        case MSG_HELLO:
            // Exactly the code: anything longer is not a hello, whatever its first byte says.
            if (length == 1)
            {
                message.command = Command::HELLO;
            }
            break;
        case MSG_VOICE_SETTINGS:
            if (length >= 3)
            {
                message.command = Command::VOICE_SETTINGS;
                message.mute = (body[1] != 0x00);
                message.deafen = (body[2] != 0x00);
            }
            break;
        case MSG_RGB:
            return parseRgb(body, length);
        case MSG_REBOOT_TO_BOOTLOADER:
            if (length == 1 + sizeof(REBOOT_MAGIC) && memcmp(body + 1, REBOOT_MAGIC, sizeof(REBOOT_MAGIC)) == 0)
            {
                message.command = Command::REBOOT_TO_BOOTLOADER;
            }
            break;
        case MSG_DEVICE_INFO:
        case MSG_BUTTON:
            // Sent by this device, never received by it.
        default:
            break;
        }
        return message;
    }
}
