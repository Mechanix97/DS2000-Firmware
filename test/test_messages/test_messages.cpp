// Message parsing tests, run on the host with `pio test -e native`.
//
// Each test parses a decoded body, as decodeFrame leaves it, and checks which command it becomes.
// A body the device must not act on parses to Command::NONE.

#include <unity.h>

#include "messages.h"

using namespace messages;

void setUp() {}
void tearDown() {}

/// Parses the bytes given, as one body.
#define PARSE(...)                                                                                 \
    ([]() {                                                                                        \
        const uint8_t body[] = {__VA_ARGS__};                                                      \
        return parse(body, sizeof(body));                                                          \
    }())

static void assertNone(const Message &message)
{
    TEST_ASSERT_EQUAL(static_cast<int>(Command::NONE), static_cast<int>(message.command));
}

static void assertCommand(Command expected, const Message &message)
{
    TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(message.command));
}

void test_an_empty_body_is_nothing()
{
    assertNone(parse(nullptr, 0));
}

void test_a_hello_is_exactly_its_code()
{
    assertCommand(Command::HELLO, PARSE(MSG_HELLO));
    assertNone(PARSE(MSG_HELLO, 0x00));
}

void test_voice_settings_read_any_non_zero_byte_as_on()
{
    const Message both = PARSE(MSG_VOICE_SETTINGS, 0x01, 0x02);
    assertCommand(Command::VOICE_SETTINGS, both);
    TEST_ASSERT_TRUE(both.mute);
    TEST_ASSERT_TRUE(both.deafen);

    const Message neither = PARSE(MSG_VOICE_SETTINGS, 0x00, 0x00);
    assertCommand(Command::VOICE_SETTINGS, neither);
    TEST_ASSERT_FALSE(neither.mute);
    TEST_ASSERT_FALSE(neither.deafen);
}

void test_truncated_voice_settings_are_dropped()
{
    assertNone(PARSE(MSG_VOICE_SETTINGS, 0x01));
}

void test_fixed_rgb_carries_both_colours()
{
    const Message message = PARSE(MSG_RGB, 200, RGB_MODE_FIXED, 128, 1, 2, 3, 4, 5, 6);
    assertCommand(Command::RGB, message);
    TEST_ASSERT_EQUAL_UINT8(200, message.brightness);
    TEST_ASSERT_EQUAL_UINT8(RGB_MODE_FIXED, message.mode);
    TEST_ASSERT_EQUAL_UINT8(128, message.speed);
    TEST_ASSERT_TRUE(message.hasColours);
    TEST_ASSERT_EQUAL_UINT8(1, message.muteColour.red);
    TEST_ASSERT_EQUAL_UINT8(2, message.muteColour.green);
    TEST_ASSERT_EQUAL_UINT8(3, message.muteColour.blue);
    TEST_ASSERT_EQUAL_UINT8(4, message.deafenColour.red);
    TEST_ASSERT_EQUAL_UINT8(5, message.deafenColour.green);
    TEST_ASSERT_EQUAL_UINT8(6, message.deafenColour.blue);
}

void test_breathing_rgb_carries_colours_too()
{
    const Message message = PARSE(MSG_RGB, 255, RGB_MODE_BREATHING, 10, 9, 9, 9, 8, 8, 8);
    assertCommand(Command::RGB, message);
    TEST_ASSERT_TRUE(message.hasColours);
    TEST_ASSERT_EQUAL_UINT8(8, message.deafenColour.blue);
}

void test_rainbow_rgb_needs_no_colours()
{
    const Message message = PARSE(MSG_RGB, 100, RGB_MODE_RAINBOW, 50);
    assertCommand(Command::RGB, message);
    TEST_ASSERT_EQUAL_UINT8(RGB_MODE_RAINBOW, message.mode);
    TEST_ASSERT_FALSE(message.hasColours);
}

/// A truncated frame used to land its brightness and mode before bailing out.
void test_fixed_rgb_missing_a_colour_byte_is_dropped_whole()
{
    assertNone(PARSE(MSG_RGB, 200, RGB_MODE_FIXED, 128, 1, 2, 3, 4, 5));
}

void test_rgb_without_its_speed_is_dropped()
{
    assertNone(PARSE(MSG_RGB, 200, RGB_MODE_RAINBOW));
}

void test_an_unknown_rgb_mode_is_dropped_whole()
{
    assertNone(PARSE(MSG_RGB, 200, 0x03, 128, 1, 2, 3, 4, 5, 6));
}

void test_reboot_needs_the_exact_magic()
{
    assertCommand(Command::REBOOT_TO_BOOTLOADER, PARSE(MSG_REBOOT_TO_BOOTLOADER, 'B', 'O', 'O', 'T'));
    assertNone(PARSE(MSG_REBOOT_TO_BOOTLOADER));
    assertNone(PARSE(MSG_REBOOT_TO_BOOTLOADER, 'B', 'O', 'O', 'X'));
    assertNone(PARSE(MSG_REBOOT_TO_BOOTLOADER, 'B', 'O', 'O', 'T', 0x00));
}

void test_messages_the_device_sends_are_ignored_when_received()
{
    assertNone(PARSE(MSG_DEVICE_INFO, 1, 1, 0, 2, 1));
    assertNone(PARSE(MSG_BUTTON, BUTTON_MUTE));
}

void test_an_unknown_code_is_ignored()
{
    assertNone(PARSE(0x7F));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_an_empty_body_is_nothing);
    RUN_TEST(test_a_hello_is_exactly_its_code);
    RUN_TEST(test_voice_settings_read_any_non_zero_byte_as_on);
    RUN_TEST(test_truncated_voice_settings_are_dropped);
    RUN_TEST(test_fixed_rgb_carries_both_colours);
    RUN_TEST(test_breathing_rgb_carries_colours_too);
    RUN_TEST(test_rainbow_rgb_needs_no_colours);
    RUN_TEST(test_fixed_rgb_missing_a_colour_byte_is_dropped_whole);
    RUN_TEST(test_rgb_without_its_speed_is_dropped);
    RUN_TEST(test_an_unknown_rgb_mode_is_dropped_whole);
    RUN_TEST(test_reboot_needs_the_exact_magic);
    RUN_TEST(test_messages_the_device_sends_are_ignored_when_received);
    RUN_TEST(test_an_unknown_code_is_ignored);
    return UNITY_END();
}
