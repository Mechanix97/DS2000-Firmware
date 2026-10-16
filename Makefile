# Thin wrappers over PlatformIO. Pick the board with ENV (see the environments in platformio.ini)
# and the PlatformIO command with PIO, e.g. `make upload ENV=pico PIO="python -m platformio"` when
# `pio` is not on the PATH.
ENV ?= rp2350_zero
PIO ?= pio

.PHONY: build upload test clean

build:
	$(PIO) run -e $(ENV)

# PlatformIO builds first, then reboots the board into USB boot and flashes it with picotool, so
# there is no drive letter to know. A board not running this firmware needs BOOT held while it is
# plugged in (see the README).
upload:
	$(PIO) run -e $(ENV) -t upload

test:
	$(PIO) test -e native

clean:
	$(PIO) run -e $(ENV) -t clean
