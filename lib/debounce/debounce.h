#pragma once

/// Key debouncing on the pin's state rather than on its edges.
///
/// A key counts as pressed once its pin has read pressed continuously for SETTLE_MS, and as
/// released once it has read released for as long. Bounces on either transition are shorter than
/// that, so they never change the settled state: one press of the key is one press, however long
/// it is held.
///
/// The previous scheme accepted a falling edge and then ignored the pin for 250 ms. A key held
/// past the lockout came back on release, whose bounce has falling edges of its own, as a second
/// press; and two deliberate presses inside 250 ms lost the second one.
///
/// This has no Arduino dependency so the native unit tests can feed it bouncing sequences.
namespace debounce
{
    /// How long a new level has to hold before it counts. Kailh Choc V1 switches are specified to
    /// bounce for at most 5 ms; 10 ms leaves margin and is still far below a deliberate press.
    const unsigned long SETTLE_MS = 10;

    class Key
    {
    public:
        /// Feeds one reading of the pin, taken at `now` (milliseconds, wrapping like millis()).
        /// Returns true exactly once per press: on the reading that settles it as pressed.
        bool update(bool pressed, unsigned long now);

    private:
        /// The level the key has settled at.
        bool settled = false;
        /// The level of the latest reading, and when the readings started showing it.
        bool reading = false;
        unsigned long readingSince = 0;
    };
}
