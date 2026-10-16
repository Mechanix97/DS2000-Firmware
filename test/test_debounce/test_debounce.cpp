// Debounce tests, run on the host with `pio test -e native`.
//
// Each test plays a sequence of pin readings into a Key, one per millisecond as loop() would take
// them (it actually reads far more often), and counts the presses it reports.

#include <unity.h>

#include "debounce.h"

using debounce::Key;
using debounce::SETTLE_MS;

void setUp() {}
void tearDown() {}

/// Holds the pin at `pressed` for `ms` milliseconds starting at `now`, counting presses.
static int hold(Key &key, unsigned long &now, bool pressed, unsigned long ms)
{
    int presses = 0;
    for (unsigned long end = now + ms; now != end; now++)
    {
        presses += key.update(pressed, now);
    }
    return presses;
}

/// A switch bouncing: the pin alternates every millisecond for `ms`, starting at `firstLevel`.
static int bounce(Key &key, unsigned long &now, bool firstLevel, unsigned long ms)
{
    int presses = 0;
    bool level = firstLevel;
    for (unsigned long end = now + ms; now != end; now++)
    {
        presses += key.update(level, now);
        level = !level;
    }
    return presses;
}

void test_a_clean_press_registers_once_after_it_settles()
{
    Key key;
    unsigned long now = 1000;
    TEST_ASSERT_EQUAL(0, hold(key, now, false, 50));
    TEST_ASSERT_EQUAL(0, hold(key, now, true, SETTLE_MS));
    TEST_ASSERT_EQUAL(1, hold(key, now, true, 1));
    TEST_ASSERT_EQUAL(0, hold(key, now, true, 100));
}

void test_bounce_on_press_registers_once()
{
    Key key;
    unsigned long now = 1000;
    hold(key, now, false, 50);
    int presses = bounce(key, now, true, 5);
    presses += hold(key, now, true, 100);
    TEST_ASSERT_EQUAL(1, presses);
}

/// The bug in #11: held past the old 250 ms lockout, the release bounce came back as a press.
void test_holding_two_seconds_and_releasing_with_bounce_is_one_press()
{
    Key key;
    unsigned long now = 1000;
    hold(key, now, false, 50);
    int presses = bounce(key, now, true, 5);
    presses += hold(key, now, true, 2000);
    presses += bounce(key, now, false, 5);
    presses += hold(key, now, false, 500);
    TEST_ASSERT_EQUAL(1, presses);
}

void test_two_presses_150_ms_apart_both_register()
{
    Key key;
    unsigned long now = 1000;
    hold(key, now, false, 50);
    int presses = bounce(key, now, true, 5);
    presses += hold(key, now, true, 60);
    presses += bounce(key, now, false, 5);
    presses += hold(key, now, false, 80);
    presses += bounce(key, now, true, 5);
    presses += hold(key, now, true, 60);
    presses += hold(key, now, false, 100);
    TEST_ASSERT_EQUAL(2, presses);
}

void test_a_glitch_shorter_than_the_settle_time_is_ignored()
{
    Key key;
    unsigned long now = 1000;
    hold(key, now, false, 50);
    int presses = hold(key, now, true, SETTLE_MS - 1);
    presses += hold(key, now, false, 100);
    TEST_ASSERT_EQUAL(0, presses);
}

void test_a_press_across_the_millis_wrap_registers_once()
{
    Key key;
    unsigned long now = (unsigned long)-5;
    hold(key, now, false, 3);
    int presses = hold(key, now, true, 50);
    presses += hold(key, now, false, 50);
    TEST_ASSERT_EQUAL(1, presses);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_a_clean_press_registers_once_after_it_settles);
    RUN_TEST(test_bounce_on_press_registers_once);
    RUN_TEST(test_holding_two_seconds_and_releasing_with_bounce_is_one_press);
    RUN_TEST(test_two_presses_150_ms_apart_both_register);
    RUN_TEST(test_a_glitch_shorter_than_the_settle_time_is_ignored);
    RUN_TEST(test_a_press_across_the_millis_wrap_registers_once);
    return UNITY_END();
}
