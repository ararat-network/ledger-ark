# Installing without Ledger Live

For a member who holds a Ledger device and wants the Ark app on it before Ledger lists the app in Ledger Live, and for
whoever cuts the release they install. The first member tranche runs on this path: the Ark app's Ledger adapter and
`arkd` drive the same device app, and the Ledger Live listing follows its catalogue review rather than gating
registration.

## What sideloading is

Ledger Live installs only apps Ledger has signed. Until that signature exists, the app is loaded over USB by
`ledgerblue`, Ledger's own loader, from an installer this repository releases. The device knows the app is unsigned:
it shows an unverified-app warning at install and asks for the PIN. Installing never exposes the seed. An app reaches
keys only through BOLOS syscalls, inside the derivation paths its install manifest declares, `44'/330'` and `44'/118'`
for this app, and the bytes loaded are the ones the release's checksum names.

Three limits follow from the device, not from the app:

- **The Nano X cannot sideload.** Its OS installs signed apps only. The Nano S+, Stax, Flex, and Apex P can.
- **An installer fits one firmware version.** The device accepts an app built at its firmware's API level and refuses
  any other with `0x511f`. Each release states the firmware version each installer was built for; a device on another
  version waits for a release built at its level.
- **A firmware update removes it.** Updating the device through Ledger Live removes every installed app and reinstalls
  the signed ones, which this app is not. After an update, install again from a release built for the new firmware.

## Before you start

- A supported device on the firmware version the release names, connected by USB and unlocked on its dashboard. Quit
  Ledger Live first: it holds the USB transport, and every command below fails while it runs.
- macOS or Linux with Python 3. Linux needs Ledger's udev rules
  ([connection guide](https://support.ledger.com/hc/en-us/articles/115005165269-Fix-connection-issues)).
- `ledgerblue` in its own virtual environment, installed once:

```sh
python3 -m venv ~/.ledger-venv && ~/.ledger-venv/bin/pip install ledgerblue
```

The device reports its firmware version from the dashboard without installing anything:

```sh
~/.ledger-venv/bin/python3 -c 'from ledgerblue.comm import getDongle; r = bytes(getDongle().exchange(bytes.fromhex("e001000000"))); print(r[5:5 + r[4]].decode())'
```

## Install

1. From the [release](https://github.com/ararat-network/ledger-ark/releases), download `SHA256SUMS` and the installer
   for your device:

   | Device  | Installer                 |
   | ------- | ------------------------- |
   | Nano S+ | `installer_nanos_plus.sh` |
   | Stax    | `installer_stax.sh`       |
   | Flex    | `installer_flex.sh`       |
   | Apex P  | `installer_apex_p.sh`     |

2. Check the download. The checksum catches a corrupted or substituted file; authenticity rests on the release page
   itself, under the `ararat-network` organisation, since a release's assets cannot change once it is published.

   ```sh
   shasum -a 256 --check --ignore-missing SHA256SUMS
   ```

   On Linux, `sha256sum --check --ignore-missing SHA256SUMS`.

3. Load it. The installer is a self-contained script carrying the app image and its load parameters, and it deletes
   an earlier Ark app before loading. The device shows the unverified-app warning; enter the PIN to accept.

   ```sh
   chmod +x ./installer_flex.sh
   PATH=~/.ledger-venv/bin:$PATH ./installer_flex.sh load
   ```

4. Check the device. The Ark icon is on the dashboard. Open the app and read the version under its settings: it
   equals the release tag, and it is 2.1.0 or later, the floor the Cosmos ledger library behind every Go client,
   `arkd` included, accepts before deriving an address.

## Use it

The app on the device is what the Ark app's Ledger adapter talks to, and what `arkd` drives through its ledger
keyring. From the CLI:

```sh
arkd keys add NAME --ledger
```

derives the `44'/330'` account, and `--coin-type 118` the legacy Terra path. Transactions sign with
`--sign-mode amino-json`, the sign document the device displays. [DEVICE_TESTING.md](DEVICE_TESTING.md) stage 2 walks
the flow against a local chain.

## Update or remove

- **A newer release:** download its installer and run `load` again. The installer removes the installed app first.
  Keys are unaffected: they derive from the seed, not from the app.
- **Remove:** `./installer_flex.sh delete`, with the installer for your device.
- **After a firmware update:** install again from a release built for the new firmware. The previous installer fails
  with `0x511f`.

## Troubleshooting

| Symptom                                       | Cause                                                                                                       |
| --------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| `0x511f` on load                              | The installer was built for another firmware version.                                                       |
| `0x5515`                                      | The device is locked; unlock it to the dashboard.                                                           |
| No device found                               | Ledger Live is running, the device is inside an app rather than on the dashboard, or Linux lacks the udev rules. |
| "ledgerblue is missing"                       | The installer runs `python3 -m ledgerblue`; put the virtual environment's `bin` first on `PATH`, as above.  |
| `hidapi: unknown failure` from `arkd`         | macOS lost the device mid-exchange; quit and reopen the Ark app on the device and rerun.                     |
| DO NOT USE on the app's home screen           | A demo build, `PRODUCTION_BUILD=0`. The signing code is the same; the banner marks it unfit for keys that hold funds. |

## What a release carries

[release.yml](../.github/workflows/release.yml) runs on a `v*` tag that matches [app/Makefile.version](../app/Makefile.version),
builds one installer per sideloadable device, writes `SHA256SUMS` over them, and publishes an immutable release whose
notes carry the firmware table.

- **It is a production build.** `PRODUCTION_BUILD=1`, the app Makefile's default, draws the Ark home screen;
  `PRODUCTION_BUILD=0`, for a local demo build, draws DO NOT USE over it. The device's own unverified-app warning stays
  either way until Ledger lists the app.
- **Its notes state the firmware.** [build_installers.sh](../.github/scripts/build_installers.sh) builds in Ledger's
  builder image with every device SDK at the tag `SDK_TAG` in the workflow names, which sets the API level, so each
  installer fits one firmware version and carries the level it was built at.
  [release_notes.py](../.github/scripts/release_notes.py) turns that level into the firmware version per device through
  [api_levels.json](../api_levels.json), a copy of the table in
  [ledger-secure-sdk](https://github.com/LedgerHQ/ledger-secure-sdk/blob/master/api_levels.json). A level with no
  production firmware for its device in the copy fails the release rather than publishing without the line; the fix is
  to refresh the copy. A firmware release from Ledger is a reason to move `SDK_TAG` and cut a new tag. The emulator
  suite runs the app at the Zondax builder image's own level, so the hardware pass on the released installers is what
  covers the level members run.

Before a release goes to members, the hardware pass in [DEVICE_TESTING.md](DEVICE_TESTING.md) runs on the devices the
release targets from the released installers, not from a local build.
