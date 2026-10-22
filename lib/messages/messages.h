#pragma once

#include <stddef.h>
#include <stdint.h>

/// The messages carried inside a frame: their codes, payloads and what the device accepts.
///
/// Parsing is kept apart from acting on a message so it runs in the native unit tests: every rule
/// about which frames are taken and which are dropped is checked on the host, without a board.
/// main.cpp only applies the command a body parses to.
///
/// This has no Arduino dependency, like the framing in lib/protocol.
namespace messages
{
    /// Message codes shared with the desktop application. Changing one breaks every device already
    /// flashed, so they are part of the contract rather than an implementation detail.
    enum MessageCode : uint8_t
    {
        /// The application opening a session. Answered with MSG_DEVICE_INFO.
        MSG_HELLO = 0x00,
        /// `[protocol][board][major][minor][patch]`: what the application decides compatibility on.
        MSG_DEVICE_INFO = 0x01,
        MSG_BUTTON = 0x02,
        MSG_VOICE_SETTINGS = 0x03,
        MSG_RGB = 0x04,
        /// Reboot into USB boot for a firmware update. Payload is REBOOT_MAGIC.
        MSG_REBOOT_TO_BOOTLOADER = 0x05,
    };

    /// Required payload of MSG_REBOOT_TO_BOOTLOADER. The CRC already rejects corrupted frames, but
    /// a message that takes the device offline should not hang on a single code byte either.
    const uint8_t REBOOT_MAGIC[] = {'B', 'O', 'O', 'T'};

    enum ButtonId : uint8_t
    {
        BUTTON_MUTE = 0x00,
        BUTTON_DEAFEN = 0x01,
        BUTTON_DISCONNECT = 0x02,
    };

    enum RgbMode : uint8_t
    {
        RGB_MODE_RAINBOW = 0x00,
        RGB_MODE_FIXED = 0x01,
        RGB_MODE_BREATHING = 0x02,
    };

    struct Colour
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    /// What a body asks the device to do. Anything the device does not accept parses to NONE.
    enum class Command : uint8_t
    {
        NONE,
        HELLO,
        VOICE_SETTINGS,
        RGB,
        REBOOT_TO_BOOTLOADER,
    };

    /// A parsed body. Only the fields of its command are set.
    struct Message
    {
        Command command = Command::NONE;

        /// VOICE_SETTINGS.
        bool mute = false;
        bool deafen = false;

        /// RGB. The colours are set only when hasColours: the rainbow owns its colour, so its frame
        /// carries none, and the colours last configured stay in place under it.
        uint8_t brightness = 0;
        uint8_t mode = RGB_MODE_FIXED;
        uint8_t speed = 0;
        bool hasColours = false;
        Colour muteColour = {0, 0, 0};
        Colour deafenColour = {0, 0, 0};
    };

    /// Parses one decoded body: the message code and its payload, CRC already checked and removed.
    Message parse(const uint8_t *body, size_t length);
}
