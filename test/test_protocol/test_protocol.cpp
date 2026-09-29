// Framing tests, run on the host with `pio test -e native`.
//
// The byte vectors are the "Reference frames" of the protocol spec, docs/PROTOCOL.md in the
// application repository, and the same ones it asserts in serial_message.rs, so the two
// implementations are checked against each other rather than each only against itself.

#include <string.h>
#include <unity.h>

#include "protocol.h"

using namespace protocol;

void setUp() {}
void tearDown() {}

static void assertFrame(const uint8_t *body, size_t length, const uint8_t *expected, size_t expectedLength)
{
    uint8_t out[MAX_ENCODED_FRAME];
    TEST_ASSERT_EQUAL_size_t(expectedLength, encodeFrame(body, length, out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, expectedLength);
}

void test_crc_matches_the_published_check_value()
{
    const char *check = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16((const uint8_t *)check, strlen(check)));
}

void test_frames_match_the_reference_wire_bytes()
{
    const uint8_t hello[] = {0x00};
    const uint8_t helloWire[] = {0x01, 0x03, 0xE1, 0xF0, 0x00};
    assertFrame(hello, sizeof(hello), helloWire, sizeof(helloWire));

    // Protocol 1, board 0x01 (RP2350-Zero), firmware 0.2.0.
    const uint8_t deviceInfo[] = {0x01, 1, 0x01, 0, 2, 0};
    const uint8_t deviceInfoWire[] = {0x04, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0xF1, 0x37, 0x00};
    assertFrame(deviceInfo, sizeof(deviceInfo), deviceInfoWire, sizeof(deviceInfoWire));

    const uint8_t reboot[] = {0x05, 'B', 'O', 'O', 'T'};
    const uint8_t rebootWire[] = {0x08, 0x05, 0x42, 0x4F, 0x4F, 0x54, 0x87, 0xB0, 0x00};
    assertFrame(reboot, sizeof(reboot), rebootWire, sizeof(rebootWire));

    const uint8_t mute[] = {0x02, 0x00};
    const uint8_t muteWire[] = {0x02, 0x02, 0x03, 0x7B, 0x6D, 0x00};
    assertFrame(mute, sizeof(mute), muteWire, sizeof(muteWire));
}

void test_255_and_zero_survive_a_round_trip()
{
    const uint8_t body[] = {0x04, 0xFF, 0x01, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
    uint8_t wire[MAX_ENCODED_FRAME];
    const size_t length = encodeFrame(body, sizeof(body), wire);

    for (size_t i = 0; i + 1 < length; i++)
    {
        TEST_ASSERT_NOT_EQUAL(FRAME_DELIMITER, wire[i]);
    }

    size_t bodyLength = 0;
    TEST_ASSERT_TRUE(decodeFrame(wire, length - 1, bodyLength));
    TEST_ASSERT_EQUAL_size_t(sizeof(body), bodyLength);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(body, wire, sizeof(body));
}

void test_a_lost_or_flipped_byte_is_rejected()
{
    const uint8_t mute[] = {0x02, 0x00};
    uint8_t wire[MAX_ENCODED_FRAME];
    const size_t frameLength = encodeFrame(mute, sizeof(mute), wire) - 1;

    for (size_t missing = 0; missing < frameLength; missing++)
    {
        uint8_t damaged[MAX_ENCODED_FRAME];
        size_t n = 0;
        for (size_t i = 0; i < frameLength; i++)
        {
            if (i != missing)
            {
                damaged[n++] = wire[i];
            }
        }
        size_t bodyLength = 0;
        TEST_ASSERT_FALSE(decodeFrame(damaged, n, bodyLength));
    }

    for (size_t flipped = 0; flipped < frameLength; flipped++)
    {
        uint8_t damaged[MAX_ENCODED_FRAME];
        memcpy(damaged, wire, frameLength);
        damaged[flipped] ^= 0x10;
        size_t bodyLength = 0;
        TEST_ASSERT_FALSE(decodeFrame(damaged, frameLength, bodyLength));
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_the_published_check_value);
    RUN_TEST(test_frames_match_the_reference_wire_bytes);
    RUN_TEST(test_255_and_zero_survive_a_round_trip);
    RUN_TEST(test_a_lost_or_flipped_byte_is_rejected);
    return UNITY_END();
}
