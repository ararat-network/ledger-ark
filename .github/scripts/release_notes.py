#!/usr/bin/env python3
"""Write the release notes' tables from the built installers and emulator binaries.

Each installer carries the API level it was built at, and api_levels.json, a copy
of the SDK's, names the OS versions at that level. Each emulator binary carries
its device, app version, and SDK in the ledger.* sections the SDK links in.
Exits non-zero when a device's level has no production firmware in the copy, so
a release never publishes without the line members check before loading, and
when an emulator binary is missing or is not the device and version its name
says.
"""

import json
import pathlib
import re
import struct
import sys

# Installer in dist/, device name, and the device's prefix in api_levels.json.
DEVICES = [
    ("installer_nanos_plus.sh", "Nano S+", "nanos+_"),
    ("installer_stax.sh", "Stax", "stax_"),
    ("installer_flex.sh", "Flex", "flex_"),
    ("installer_apex_p.sh", "Apex P", "apex_p_"),
]

# Zemu's model name, which the binary's file name carries, device name, and the
# target the SDK records in the ELF.
EMULATED = [
    ("nanox", "Nano X", "nanox"),
    ("nanosp", "Nano S+", "nanos2"),
    ("stax", "Stax", "stax"),
    ("flex", "Flex", "flex"),
    ("apex_p", "Apex P", "apex_p"),
]


def firmware(levels, level, prefix):
    """Production OS versions for one device at one API level, in the SDK's order."""
    return [
        v[len(prefix) :]
        for v in levels.get(level, [])
        if v.startswith(prefix) and "-" not in v
    ]


def ledger_sections(path):
    """The ledger.* sections of a device app's ELF (32-bit, little-endian), as text."""
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        sys.exit(f"{path.name}: not a 32-bit little-endian ELF")
    (table,) = struct.unpack_from("<I", data, 32)
    size, count, names_index = struct.unpack_from("<HHH", data, 46)
    # Per section: the offset of its name in the names section, its offset, its length.
    headers = [
        struct.unpack_from("<I12xII", data, table + i * size) for i in range(count)
    ]
    names = headers[names_index][1]
    sections = {}
    for name_at, offset, length in headers:
        start = names + name_at
        name = data[start : data.index(b"\0", start)].decode()
        if name.startswith("ledger."):
            sections[name[len("ledger.") :]] = (
                data[offset : offset + length].rstrip(b"\0").decode()
            )
    return sections


def installer_rows(dist, levels, levels_path):
    rows = []
    for name, device, prefix in DEVICES:
        text = (dist / name).read_text()
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
    return rows


def emulator_rows(dist, tag):
    rows = []
    for model, device, target in EMULATED:
        name = f"ark_zemu_{model}_{tag}.elf"
        path = dist / name
        if not path.is_file():
            sys.exit(f"{name}: missing from {dist}")
        built = ledger_sections(path)
        for section, expected in (("target", target), ("app_version", tag[1:])):
            if built.get(section) != expected:
                sys.exit(
                    f"{name}: its {section} is {built.get(section)!r}, not {expected!r}"
                )
        rows.append(
            f"| {device} | `{model}` | `{name}` | {built.get('sdk_version', '')} "
            f"| {built.get('api_level', '')} |"
        )
    return rows


def main(dist, levels_path, tag):
    dist = pathlib.Path(dist)
    levels = json.loads(pathlib.Path(levels_path).read_text())
    installers = installer_rows(dist, levels, levels_path)
    emulated = emulator_rows(dist, tag)

    repo = f"https://github.com/ararat-network/ledger-ark/blob/{tag}"
    print(
        f"Installers for sideloading, one per device; the Nano X cannot sideload. Check each "
        f"download against `SHA256SUMS` and follow the [install guide]({repo}/docs/SIDELOAD.md). "
        f"An installer loads only on the firmware it was built for:\n\n"
        "| Device | Installer | Firmware | API level |\n"
        "| --- | --- | --- | --- |\n" + "\n".join(installers) + "\n\n"
        "Emulator binaries, one per device the emulator tests run on: the app as "
        "[Zemu](https://github.com/Zondax/zemu) loads it, for testing software that talks to "
        "the app. They are not installers and are not what a member loads on a device. They "
        "come from the emulator build, a separate build from the installers', and the emulator "
        "suite passed on these files in the run that published this release. To "
        f"[test against them]({repo}/README.md#testing-against-a-released-app), pin this tag "
        "and each file's SHA-256, which `SHA256SUMS` lists:\n\n"
        "| Device | Zemu model | Binary | SDK | API level |\n"
        "| --- | --- | --- | --- | --- |\n" + "\n".join(emulated)
    )


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: release_notes.py DIST_DIR API_LEVELS_JSON TAG")
    main(*sys.argv[1:])
