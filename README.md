<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/ark_dark.svg">
  <img src="docs/ark_light.svg" alt="Ark" width="280">
</picture>

# Ledger Ark app

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![GithubActions](https://github.com/ararat-network/ledger-ark/actions/workflows/main.yml/badge.svg)](https://github.com/ararat-network/ledger-ark/blob/main/.github/workflows/main.yml)

This project contains the Ark app for Ledger Nano S+, X, Stax, Flex and Apex P.

It is a fork of [cosmos/ledger-cosmos](https://github.com/cosmos/ledger-cosmos) (Zondax) rebranded for the
Ark chain: derivation on the Terra coin type 330' (with 118' kept for legacy Terra wallets), the `ark`
bech32 prefix, `ark-1` as the default chain id, and NOAH denomination display. The Ethereum-style 60'
path and the Ledger Live swap integration were removed.

- Ledger Nano S+/X/Stax/Flex/Apex P Ark app
- Specs / Documentation
- C++ unit tests
- Zemu tests

## ATTENTION

Please:

- **Do not use in production**
- **Do not use a Ledger device with funds for development purposes.**
- **Have a separate and marked device that is used ONLY for development and testing**

Tip:

- Releases carry precompiled test installers. If you are just curious, you can load one and avoid building.

## Download and install a prerelease

*Once the app is approved by Ledger, it will be available in their app store (Ledger Live).
Until then, each `v*` tag publishes a [release](https://github.com/ararat-network/ledger-ark/releases) with demo
builds; the app's home screen reads DO NOT USE. THESE ARE UNVETTED DEVELOPMENT RELEASES*

Download the installer for your device (`installer_nanos_plus.sh`, `installer_stax.sh`, `installer_flex.sh` or
`installer_apex_p.sh`; the Nano X cannot sideload) together with `SHA256SUMS`, check it, and load it. The
installers need `ledgerblue` (see [Preconditions](#preconditions)):

```sh
sha256sum --check --ignore-missing SHA256SUMS
chmod +x ./installer_nanos_plus.sh
./installer_nanos_plus.sh load
```

# Development

## Preconditions

- Be sure you checkout submodules too:

    ```
    git submodule update --init --recursive
    ```

- Install Docker CE. Device builds run in Zondax's builder image and use the Ledger SDKs it ships.
    - Instructions can be found here: https://docs.docker.com/install/

- CI runs on Ubuntu, which needs:
   ```
   sudo apt-get update && sudo apt-get -y install build-essential git cmake python3 python3-pip python-is-python3 libusb-1.0-0 libudev-dev
   ```

- Install Node 20.19 or later (CI uses 22) and run `corepack enable`: the Zemu targets fetch their pinned pnpm
  through corepack.

- With Python 3 as `python`, run
    - `make deps`

*Warning*: `make deps` installs `ledgerblue` with whichever `pip` is on your `PATH`, while the installers run
`python3 -m ledgerblue`. If an installer reports that ledgerblue is missing, the two point at different interpreters.

## How to build ?

> We like clion or vscode but let's have some reproducible command line steps
>

- Building the app itself

    If you installed the what is described above, just run:
    ```bash
    make
    ```

    This builds every device target, leaving the ELFs in `app/output/` and the installers in `app/pkg/`.

## Running tests

- Running C/C++ tests (x64)

    If you installed the what is described above, just run:
    ```bash
    make cpp_test
    ```

- Running device emulation+integration tests!!

   ```bash
    Use Zemu! Explained below!
    ```

## How to test with Zemu?

> [Zemu](https://github.com/Zondax/zemu) ([npm](https://www.npmjs.com/package/@zondax/zemu)) is Zondax's
> emulation and testing framework for Ledger apps.

Let's go! First install everything:
> At this moment, if you change the app you will need to run `make` before running the test again.

```bash
make zemu_install
```

Then you can run JS tests:

```bash
make zemu_test
```

`make test_all` does all three steps, as CI does. To run a single test, pass its name to jest:

```bash
cd tests_zemu && corepack pnpm exec jest -t 'get address'
```

## Using a real device

> **Please do not use a Ledger device with funds for development purposes.**
>
> **Have a separate and marked device that is used ONLY for development and testing**

[docs/DEVICE_TESTING.md](docs/DEVICE_TESTING.md) walks through sideloading the app and verifying it on hardware,
end to end against a local Ark chain. On Linux hosts the device may need udev rules first: see Ledger's
[connection guide](https://support.ledger.com/hc/en-us/articles/115005165269-Fix-connection-issues).

To load a local build, run `make`, then the installer for your device. _Warning: the installer deletes the
installed app before loading._

```
make loadS2                           # Nano S+
make loadST                           # Stax
make loadFL                           # Flex
./app/pkg/installer_apex_p.sh load    # Apex P (zxlib's loadAP looks for installer_apex.sh)
```

## Documentation

- [APDU Protocol](docs/APDUSPEC.md)
- [Transaction format](docs/TXSPEC.md)
- [Hardware verification](docs/DEVICE_TESTING.md)
- [Security policy](SECURITY.md)
