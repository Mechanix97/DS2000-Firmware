# Changelog

What changes on the device, release by release. Each GitHub release starts with its entry from
this file, and the Discord announcement is built from it. The pull requests behind every release
are listed in its notes on GitHub.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- An image for the DS2000 rev A board (`*_rev_a.uf2`), with its two LEDs under the mute and deafen
  keys.

## [0.2.1] - 2026-10-17

Serial protocol 1.

### Added

- Every image is signed with minisign. The DS2000 app installs only images signed with the
  project's key.

## [0.2.0] - 2026-10-17

First release. Serial protocol 1: pairs with DS2000 app 0.2.0.

### Added

- Images for the Waveshare RP2350-Zero, the Raspberry Pi Pico 2 and the Raspberry Pi Pico.
- Serial protocol 1: framed, checksummed messages and a versioned handshake with the app.
- RGB modes rainbow, fixed and breathing, with a speed control.
- The app can restart the device into USB boot to install a firmware update.
- The device shows up as DS2000 over USB instead of as the board it runs on.

### Fixed

- Holding a key down counts as one press. Releasing it could register a second one.

[Unreleased]: https://github.com/Mechanix97/DS2000-Firmware/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/Mechanix97/DS2000-Firmware/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/Mechanix97/DS2000-Firmware/releases/tag/v0.2.0
