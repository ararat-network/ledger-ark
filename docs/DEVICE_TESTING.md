# Hardware Verification

The emulator suite (`tests_zemu/`) already proves the app's logic: derivations match
independently computed vectors, refused paths are refused, and every screen is pinned
pixel-exact on all five devices. What it cannot prove is the part only hardware runs:
the real secure element, BOLOS enforcing the install manifest, USB transport, and the
`arkd` keyring speaking to the device end to end. That is what these three stages cover,
in order, each building on the previous one. Run them before treating any build as
release-grade.

The app under test is unsigned, so the device warns on install and asks for the PIN.
Installing an app never exposes the seed — apps only reach keys through BOLOS syscalls
inside their manifest-declared derivation domain — but the standard advice applies:
prefer a device you are comfortable experimenting on.

## Prerequisites

- A Ledger Flex on current firmware, connected by USB, unlocked. **Quit Ledger Live**
  first: it holds the HID transport and every command below will fail while it runs.
- Docker (for the builder image), `python3`, and Node 20+ with corepack.
- The ark chain checkout at `../ark` for stages 2–3.

## Stage 1 — Sideload and exercise the app

### Build and install

The build generates a self-contained installer per device under `app/pkg/`, with the
hex image and the load parameters (app name, both derivation paths) embedded:

```bash
docker run --rm -v "$PWD":/app -w /app/app \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c 'make clean >/dev/null; BOLOS_SDK=$FLEX_SDK make -j4'
```

Install `ledgerblue` once (a venv keeps it isolated), then load:

```bash
python3 -m venv ~/.ledger-venv && ~/.ledger-venv/bin/pip install ledgerblue
```

```bash
PATH=~/.ledger-venv/bin:$PATH ./app/pkg/installer_flex.sh load
```

The device shows the unverified-app warning and asks for the PIN. `installer_flex.sh
delete` removes the app again. If `ledgerblue` cannot see the device, check that
Ledger Live is closed and the device is unlocked on the dashboard.

### Verify on the dashboard

- [ ] The Ark icon renders on the dashboard; opening it shows the app name and,
      under settings, version 1.0.0.

### Probe the derivation domains

Run the probe from `tests_zemu/` (its `node_modules` already has the client library;
add the USB transport once):

```bash
cd tests_zemu && corepack pnpm add -D @ledgerhq/hw-transport-node-hid
```

```bash
cd tests_zemu && corepack pnpm exec ts-node -e '
import TransportNodeHid from "@ledgerhq/hw-transport-node-hid";
import ArkApp from "@zondax/ledger-cosmos-js";

async function main() {
  const app = new ArkApp(await TransportNodeHid.create());

  console.log("version:", await app.getVersion());

  const main330 = await app.getAddressAndPubKey("m/44'\''/330'\''/0'\''/0/0", "ark");
  console.log("330:", main330.bech32_address);
  const legacy118 = await app.getAddressAndPubKey("m/44'\''/118'\''/0'\''/0/0", "ark");
  console.log("118:", legacy118.bech32_address);

  for (const [path, hrp, want] of [
    ["m/44'\''/60'\''/0'\''/0/0", "ark", 0x698b],   // removed eth path
    ["m/44'\''/330'\''/0'\''/0/0", "ARK", 0x698c],  // malformed HRP
  ] as const) {
    try { await app.getAddressAndPubKey(path, hrp); console.log("NOT REFUSED:", path, hrp); }
    catch (e: any) { console.log(hrp, path.split("/")[2], "refused:", e.returnCode === want ? "ok" : e.returnCode); }
  }

  // Confirm-on-device flow: the same address must appear on the screen.
  const shown = await app.showAddressAndPubKey("m/44'\''/330'\''/0'\''/0/0", "ark");
  console.log("shown matches:", shown.bech32_address === main330.bech32_address);
}
main().then(() => process.exit(0));
'
```

- [ ] Both paths return `ark1...` addresses (they differ from each other — different
      derivation domains, same seed).
- [ ] The 60' path is refused with `0x698b` and the malformed HRP with `0x698c`.
- [ ] The show-address flow displays the identical `ark1...` string on the device, with
      the "Verify Ark address" heading and working QR page.

With the Zemu test mnemonic (`equip will roof matter pink blind book anxiety banner
elbow sun young`) the 330' account 0 address is
`ark1uayrf8zh44620zyjd052gdcjcrvjpgkk7qjclx` and the legacy one is
`ark1w34k53py5v5xyluazqpq65agyajavep2nns3mh`. With a personal seed there are no
published vectors; the check is instead that stage 2's `arkd keys add --ledger` — a
completely separate client stack — reports the same address this probe printed.

## Stage 2 — End to end against a local ark chain

### Build arkd

The chain repo's `make build` compiles the ledger keyring by default (`BUILD_TAGS ?=
ledger`, matching what goreleaser ships), so a stock build drives the device:

```bash
cd ../ark && make build
```

On a checkout from before that default, or after overriding `BUILD_TAGS`, the binary
refuses `--ledger` at runtime with "not available in this executable" — rebuild with
the tag rather than debugging the transport.

### Stand up a single-validator devnet

Use the mainnet chain id on the throwaway net on purpose: `ark-1` is the app's
default chain, so this exercises the exact production review UX (compact display,
NOAH conversion). Any other id still works but shows the chain-id/account/sequence
screens.

```bash
cd ../ark && build/arkd testnet init-files -v 1 -o ~/.ark-devnet \
  --chain-id ark-1 --keyring-backend test
```

```bash
cd ../ark && build/arkd start --home ~/.ark-devnet/node0/arkd
```

### Register the device key and fund it

With the Ark app open on the device, run these from a second terminal. `--home` is
node0's directory, whose `client.toml` (written by `init-files`) points at the devnet's
RPC:

```bash
build/arkd keys add flex --ledger --keyring-backend test --home ~/.ark-devnet/node0/arkd
```

- [ ] The address matches the stage-1 probe's 330' address.
- [ ] `keys add flex118 --ledger --coin-type 118` likewise matches the legacy probe.

Fund it from the validator account created by `init-files`, then send from the device:

```bash
build/arkd tx bank send node0 $(build/arkd keys show flex -a --keyring-backend test --home ~/.ark-devnet/node0/arkd) 1000000000000000000anoah \
  --chain-id ark-1 --keyring-backend test --home ~/.ark-devnet/node0/arkd --fees 300000000000000anoah -y
```

```bash
build/arkd tx bank send flex ark1w4efqfklkezgyt6lncjdwxncrzyzpr2eech3ul 500000000000000000anoah \
  --sign-mode amino-json --chain-id ark-1 --keyring-backend test --home ~/.ark-devnet/node0/arkd --fees 300000000000000anoah
```

`--sign-mode amino-json` is required: the device signs the legacy amino sign doc.

- [ ] The device review shows **no** chain-id/account/sequence screens (default chain,
      non-expert), the amount and fee as `NOAH` figures at 18 decimals, and `ark1...`
      addresses.
- [ ] Approving broadcasts successfully; `build/arkd q tx <hash>` shows code 0.
- [ ] A delegation (`tx staking delegate`) and a gov vote (`tx gov vote`) sign and land
      the same way.
- [ ] Rejecting on the device surfaces a clean CLI error and the next command works —
      the device is idle, not wedged mid-transaction.

## Stage 3 — Ark-specific messages

The app renders amino JSON generically, so the chain's own msgs need no device code —
but that is exactly the claim to verify: every field a user must be able to review has
to come up legible, and nothing may hit the transaction buffer unannounced. From the
funded `flex` account, walk the user-signable surface (discover exact arguments with
`tx <module> --help`; every command takes the stage-2 flags including
`--sign-mode amino-json`):

- [ ] `tx market ...` — every order a trader signs (`tx market --help`), between
      `anoah` and a stablecoin denom. The Type screen must show the short amino name,
      and the offer and ask fields, bound included, must render.
- [ ] `tx disbursement release` with one or two grant IDs, the one disbursement msg an
      ordinary account signs: the ID list must render. Without grants on the devnet it
      fails on chain, which does not matter here; the review is what is under test.
- [ ] A wasm execute (`tx wasm execute`): store and instantiate the chain repo's
      `tests/e2e/testdata/counter.wasm` (`{"count":0}`), then execute
      `{"increment":{}}`. The JSON msg body renders as paginated screens — confirm it
      stays readable, not garbage. A cw3 committee's members sign exactly this shape.
- [ ] A multisig member's signature, which is how a multisig committee signs: `keys add
      com --multisig flex,node0 --multisig-threshold 2`, fund `com`, write a
      `tx bank send com ... --generate-only` to `tx.json`, sign it with
      `tx sign tx.json --multisig com --from flex` (and again `--from node0`), then
      `tx multisign tx.json com flex.json node0.json` and `tx broadcast`. The review
      must show `com` as the sender, since it is not the device's own address, and the
      combined transaction must land.
- [ ] One deliberately large transaction — a `gov submit-proposal` with a long
      description or a wasm execute with a sizeable payload. Either it reviews cleanly
      or it fails with the explicit "Transaction is too big" error. Truncated or
      partial display is a bug; stop and file it.
- [ ] Repeat one custom msg with expert mode enabled on the device: every raw field,
      including chain id and sequence, appears.

Any msg type that renders poorly (an unreadable field name, an amount without its
denom context) is fixed chain-side — a better amino name or field name — not in the
app. Record the screen count for the largest flow; it feeds the Ledger submission's
design-guideline review.

## After a pass

A full pass here plus the committed emulator suite is the evidence base for the next
steps: an external review of the fork's delta and the Ledger Live catalog submission.
Keep the notes of which msg types were exercised on hardware — Ledger's review asks
for exactly that.
