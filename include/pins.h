#ifndef _PINS_H_
#define _PINS_H_

// GP0 and GP1 carry UART0 by default on this part, but that alternate function only comes alive
// if something calls Serial1.begin(). The link to the desktop application is `Serial`, which is
// USB CDC on the RP2350, so nothing contends for them.
#define MUTE_BUTTON 0
#define DEAF_BUTTON 1
#define DISCONNECT_BUTTON 2

#ifdef LED_DRIVER_PIXEL

// Rev A: two WS2812-compatible pixels under the mute and deafen keys, chained mute first, fed
// from GP5 through a 3.3 V to 5 V buffer. GP6-GP10 are free. An environment can point the driver
// elsewhere, like the RP2350-Zero's onboard WS2812 on GPIO16, by defining these in its
// build_flags.
#ifndef LED_DATA
#define LED_DATA 5
#endif
#ifndef LED_COUNT
#define LED_COUNT 2
#endif
// The order the pixel expects its colour bytes in: GRB, as WS2812B and SK6812 parts take it.
#ifndef LED_PIXEL_ORDER
#define LED_PIXEL_ORDER NEO_GRB
#endif

#else

// The prototype: one four-pin common-cathode RGB LED per key, a PWM pin per colour.
#define MUTE_LED_RED 5
#define MUTE_LED_GREEN 6
#define MUTE_LED_BLUE 7

#define DEAF_LED_RED 8
#define DEAF_LED_GREEN 9
#define DEAF_LED_BLUE 10

#endif

#endif
