"""Injects the firmware version into the build.

The version is written once, as `custom_firmware_version` in platformio.ini, and reaches the code
as FIRMWARE_VERSION_MAJOR/MINOR/PATCH. The device reports it in the handshake's device info, so it has to be
right: a build of a tagged commit whose tag disagrees with it is refused rather than shipped
reporting the wrong version.
"""

import re
import subprocess

Import("env")  # noqa: F821 - provided by PlatformIO's SCons environment


def fail(message):
    print(f"Error: {message}")
    env.Exit(1)


version = env.GetProjectOption("custom_firmware_version", "").strip()
match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
if not match:
    fail(f"custom_firmware_version must look like 1.2.3, got {version!r}")

parts = [int(part) for part in match.groups()]
# Each part travels as one byte of the device info payload.
if any(part > 255 for part in parts):
    fail(f"version {version}: each part must fit in a byte")

try:
    tag = subprocess.run(
        ["git", "describe", "--tags", "--exact-match", "HEAD"],
        cwd=env.subst("$PROJECT_DIR"),
        capture_output=True,
        text=True,
        check=False,
    ).stdout.strip()
except OSError:
    # No git on this machine, e.g. building from a source archive. Nothing to compare against.
    tag = ""

if tag and tag.lstrip("v") != version:
    fail(f"HEAD is tagged {tag} but platformio.ini says {version}; update one of them")

env.Append(
    CPPDEFINES=[
        ("FIRMWARE_VERSION_MAJOR", parts[0]),
        ("FIRMWARE_VERSION_MINOR", parts[1]),
        ("FIRMWARE_VERSION_PATCH", parts[2]),
    ]
)
