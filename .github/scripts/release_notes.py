#!/usr/bin/env python3
"""Write the release notes' firmware table from the built installers.

Each installer carries the API level it was built at, and api_levels.json, a copy
of the SDK's, names the OS versions at that level. Exits non-zero when a device's
level has no production firmware in the copy, so a release never publishes
without the line members check before loading.
"""

import json
import pathlib
import re
import sys

# Installer in dist/, device name, and the device's prefix in api_levels.json.
DEVICES = [
    ("installer_nanos_plus.sh", "Nano S+", "nanos+_"),
    ("installer_stax.sh", "Stax", "stax_"),
    ("installer_flex.sh", "Flex", "flex_"),
    ("installer_apex_p.sh", "Apex P", "apex_p_"),
]


def firmware(levels, level, prefix):
    """Production OS versions for one device at one API level, in the SDK's order."""
    return [
        v[len(prefix) :]
        for v in levels.get(level, [])
        if v.startswith(prefix) and "-" not in v
    ]


def main(dist, levels_path, tag):
    levels = json.loads(pathlib.Path(levels_path).read_text())
    rows = []
    for name, device, prefix in DEVICES:
        text = (pathlib.Path(dist) / name).read_text()
        found = re.search(r"--apiLevel (\d+)", text)
        if not found:
            sys.exit(f"{name}: no --apiLevel in its load parameters")
        level = found.group(1)
        versions = firmware(levels, level, prefix)
        if not versions:
            sys.exit(
                f"{name}: API level {level} has no production {device} firmware in "
                f"{levels_path}; refresh the copy from the SDK before releasing"
            )
        rows.append(f"| {device} | `{name}` | {', '.join(versions)} | {level} |")

    guide = f"https://github.com/ararat-network/ledger-ark/blob/{tag}/docs/SIDELOAD.md"
    print(
        f"Installers for sideloading, one per device; the Nano X cannot sideload. Check each "
        f"download against `SHA256SUMS` and follow the [install guide]({guide}). An installer "
        f"loads only on the firmware it was built for:\n\n"
        "| Device | Installer | Firmware | API level |\n"
        "| --- | --- | --- | --- |\n" + "\n".join(rows)
    )


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: release_notes.py DIST_DIR API_LEVELS_JSON TAG")
    main(*sys.argv[1:])
