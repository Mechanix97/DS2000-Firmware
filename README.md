<p align="center">
  <img src="docs/img/ds2000-logo.webp" alt="DS2000" width="150">
</p>

# DS-2000 firmware

Firmware for the DS-2000, a three-key Discord control deck. Built with PlatformIO for the Waveshare
RP2350-Zero (default), the Pico 2 and the Pico.

```sh
pio run            # build every environment
pio run -e pico2   # build a specific one
```

## Serial protocol

The wire format this firmware speaks with the desktop app is specified in the app repository:
[docs/PROTOCOL.md](https://github.com/Mechanix97/DS2000/blob/main/docs/PROTOCOL.md). Any change to
it bumps `PROTOCOL_VERSION` on both sides and lands as a pair of pull requests.

The version is set once, as `custom_firmware_version` in `platformio.ini`, and each board
environment sets its `custom_board_id`. Both are reported to the app in the handshake.

```sh
pio test -e native   # framing tests against the protocol's reference frames (needs gcc or clang)
```

## The DS-2000 project

- [DS2000](https://github.com/Mechanix97/DS2000): the desktop app
- [DS2000-PCB](https://github.com/Mechanix97/DS2000-PCB): the KiCad board
- [DS2000-Enclosure](https://github.com/Mechanix97/DS2000-Enclosure): the 3D-printed tray
