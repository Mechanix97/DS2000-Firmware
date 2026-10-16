<p align="center">
  <img src="docs/img/ds2000-logo.webp" alt="DS2000" width="150">
</p>

# DS2000 firmware

Firmware for the DS2000, a desk device with three keys (mute, deafen, disconnect) and two RGB
LEDs. It talks over USB to the [DS2000 app](https://github.com/Mechanix97/DS2000), which drives
Discord: a key press goes to the app, and the app sends back the voice state and the LED colours.

The mute LED is lit while you are muted or deafened, the deafen LED while you are deafened. Colour,
brightness and mode (rainbow, fixed, breathing) are set from the app.

## Boards

| Environment | Board | Board id |
|---|---|---|
| `rp2350_zero` | Waveshare RP2350-Zero (default, the current prototype) | `0x01` |
| `pico2` | Raspberry Pi Pico 2 | `0x02` |
| `pico` | Raspberry Pi Pico (RP2040) | `0x03` |

The board id is reported to the app in the handshake. The DS2000 rev A board does not have an
environment yet ([#12](https://github.com/Mechanix97/DS2000-Firmware/issues/12)).

`rp2350_zero_pixel` is a development build of `rp2350_zero` that runs the rev A LED driver on the
module's onboard WS2812 (mute LED only). It is not released.

## Installing a release

The latest images are in the [latest release](https://github.com/Mechanix97/DS2000-Firmware/releases/latest),
one `.uf2` per board.

1. Download the `.uf2` for your board. Its SHA-256 is in the release notes.
2. Hold the board's BOOT button (BOOTSEL on the Picos) while plugging it in. It shows up as a USB drive.
3. Copy the `.uf2` onto the drive. The board restarts with the new firmware.

The app and the firmware must speak the same serial protocol. Each release says which one.

## Building

Install [PlatformIO](https://platformio.org/install/cli) (`pip install platformio`, or the VS Code
extension). The first build downloads the toolchain.

```sh
pio run                        # build the default environment, rp2350_zero
pio run -e pico2               # build another one
pio run -e rp2350_zero -t upload
```

The image ends up in `.pio/build/<environment>/firmware.uf2`.

The Makefile wraps the same commands: `make build`, `make upload`, `make test` and `make clean`.
`ENV` picks the board (`make upload ENV=pico`) and `PIO` the command, if `pio` is not on the PATH
(`make upload PIO="python -m platformio"`).

`upload` reboots a board already running this firmware into USB boot by itself. Close the DS2000
app first, since it holds the serial port. A blank board, or one running something else, needs the
BOOT button as in [Installing a release](#installing-a-release); then run `upload` again or copy the
`.uf2` by hand.

## Pinout

| Function | GPIO |
|---|---|
| Mute key | GP0 |
| Deafen key | GP1 |
| Disconnect key | GP2 |

The keys are active-low, with the internal pull-ups, and go to ground.

The LEDs have two drivers, picked per environment:

| Driver | Hardware | Pins | Used by |
|---|---|---|---|
| PWM (default) | One common-cathode RGB LED per key | Mute R / G / B on GP5 / GP6 / GP7, deafen on GP8 / GP9 / GP10 | the prototype: `rp2350_zero`, `pico2`, `pico` |
| Pixel (`LED_DRIVER_PIXEL`) | SK6812MINI-E chain, mute first, GRB | Data on GP5 | rev A |

[`include/pins.h`](include/pins.h) is the source of truth: the
[PCB](https://github.com/Mechanix97/DS2000-PCB) follows it.

## Serial protocol

The wire format this firmware speaks with the desktop app is specified in the app repository:
[docs/PROTOCOL.md](https://github.com/Mechanix97/DS2000/blob/main/docs/PROTOCOL.md). Any change to
it bumps `PROTOCOL_VERSION` on both sides and lands as a pair of pull requests.

```sh
pio test -e native   # framing tests against the protocol's reference frames (needs gcc or clang)
```

## Releasing

The version is set once, as `custom_firmware_version` in `platformio.ini`. Bump it, merge, and push
a matching tag (`v0.2.0` for `0.2.0`). CI runs the tests, builds every board and attaches the images
and their checksums to a draft release. A tag that does not match `platformio.ini` fails the build.

## License

[AGPL-3.0](LICENSE).

## The DS2000 project

- [DS2000](https://github.com/Mechanix97/DS2000): the desktop app
- [DS2000-PCB](https://github.com/Mechanix97/DS2000-PCB): the KiCad board
- [DS2000-Enclosure](https://github.com/Mechanix97/DS2000-Enclosure): the 3D-printed enclosure
