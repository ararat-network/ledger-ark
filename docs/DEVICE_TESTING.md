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

Install `ledgerblue` once (a venv keeps it isolated) and read the device's firmware
version from the dashboard; the query needs no confirmation:

```bash
python3 -m venv ~/.ledger-venv && ~/.ledger-venv/bin/pip install ledgerblue
```

```bash
~/.ledger-venv/bin/python3 -c 'from ledgerblue.comm import getDongle; r = bytes(getDongle().exchange(bytes.fromhex("e001000000"))); print(r[5:5 + r[4]].decode())'
```

The device installs only an app built at the API level of its firmware (`0x511f`
otherwise), and the builder image pins its SDK at one level, so check out the matching
`API_LEVEL_<n>` branch of the image's SDK first. The level table is in the SDK's README
(LedgerHQ/ledger-secure-sdk); Flex 1.7.0 is level 27. The build generates a
self-contained installer per device under `app/pkg/`, with the hex image and the load
parameters (app name, both derivation paths) embedded:

```bash
docker run --rm -v "$PWD":/app -w /app/app \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c 'git -C $FLEX_SDK checkout -q origin/API_LEVEL_27 && make clean >/dev/null; BOLOS_SDK=$FLEX_SDK make -j4'
```

```bash
PATH=~/.ledger-venv/bin:$PATH ./app/pkg/installer_flex.sh load
```

The device shows the unverified-app warning and asks for the PIN. `installer_flex.sh
delete` removes the app again. If `ledgerblue` cannot see the device, check that
Ledger Live is closed and the device is unlocked on the dashboard; `0x5515` is the
locked dashboard.

### Verify on the dashboard

- [ ] The Ark icon renders on the dashboard; opening it shows the app name and,
      under settings, the version from `app/Makefile.version`. It must be 2.1.0 or
      later: the Cosmos ledger library behind every Go client, `arkd` included, refuses
      an older app before deriving anything.

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

The launch deposit floor is 1,000 NOAH and the devnet's whole supply is 500, so no
proposal would ever reach its vote; lower the floor and shorten the vote before the
first start:

```bash
cd ~/.ark-devnet/node0/arkd/config && jq '.app_state.gov.params.min_deposit[0].amount = "1000000000000000000" | .app_state.gov.params.voting_period = "600s"' genesis.json > g.json && mv g.json genesis.json
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

Fund it from the validator account created by `init-files`, then send from the device.
No fee flags anywhere: Treasury prices every transaction and `arkd` declares the fee
itself, so an explicit `--fees` below the live base fee is refused with code 13.

```bash
build/arkd tx bank send node0 $(build/arkd keys show flex -a --keyring-backend test --home ~/.ark-devnet/node0/arkd) 2000000000000000000anoah \
  --chain-id ark-1 --keyring-backend test --home ~/.ark-devnet/node0/arkd -y
```

```bash
build/arkd tx bank send flex ark1w4efqfklkezgyt6lncjdwxncrzyzpr2eech3ul 500000000000000000anoah \
  --sign-mode amino-json --chain-id ark-1 --keyring-backend test --home ~/.ark-devnet/node0/arkd -y
```

`--sign-mode amino-json` is required: the device signs the legacy amino sign doc. `-y`
skips the CLI's confirmation so the command reaches the device at once. The Go HID
library under `arkd` sometimes reports `hidapi: unknown failure`, which is macOS
reporting the device gone mid-exchange; quit and reopen the Ark app and rerun. Its
errors say "Cosmos app" because `arkd` does not set the app name.

- [ ] The device review shows **no** chain-id/account/sequence screens (default chain,
      non-expert), the amount and fee as `NOAH` figures at 18 decimals, and `ark1...`
      addresses.
- [ ] Approving broadcasts successfully; `build/arkd q tx <hash>` shows code 0.
- [ ] A delegation of at least 1 NOAH (`tx staking delegate <valoper> 1000000000000000000anoah`)
      signs and lands the same way. It comes first: voting needs that much staked.
- [ ] A gov vote (`tx gov vote 1 yes`) signs and lands. Put the proposal up from `node0`
      beforehand; a text proposal needs non-empty `metadata` and the patched deposit:

      ```bash
      printf '{"messages": [], "metadata": "hardware-pass", "deposit": "1000000000000000000anoah", "title": "Hardware pass", "summary": "Exists so a device-signed vote can land.", "expedited": false}\n' > proposal.json && build/arkd tx gov submit-proposal proposal.json --from node0 --chain-id ark-1 --keyring-backend test --home ~/.ark-devnet/node0/arkd -y
      ```

- [ ] Rejecting on the device surfaces the user-rejected status word as a clean CLI
      error and the next command works — the device is idle, not wedged
      mid-transaction.

## Stage 3 — Ark-specific messages

The app renders amino JSON generically, so the chain's own msgs need no device code —
but that is exactly the claim to verify: every field a user must be able to review has
to come up legible, and nothing may hit the transaction buffer unannounced. From the
funded `flex` account, walk the user-signable surface (discover exact arguments with
`tx <module> --help`; every command takes the stage-2 flags including
`--sign-mode amino-json`):

- [ ] `tx market sell 100000000000000000anoah ausd 1ausd` and the matching `buy`, the
      orders a trader signs. The Type screen must show the short amino name, and the
      offer and ask fields, bound included, must render. Without an oracle rate on the
      devnet it fails on chain, which does not matter here; the review is what is under
      test.
- [ ] `tx disbursement set-payee --grant-id 1 --payee <addr>` and `set-controller`, the
      disbursement msgs an ordinary account signs: the grant id and addresses must
      render. Without grants it fails on chain, as above.
- [ ] A wasm execute (`tx wasm execute`): store and instantiate the chain repo's
      `tests/e2e/testdata/counter.wasm` (`{"count":0}`) from `node0` — a store runs to
      133 kB, far past the app's buffer — then execute `{"increment":{}}` from `flex`
      with `--gas auto`. The JSON msg body renders as paginated screens — confirm it
      stays readable, not garbage, and that `get_count` moved. A cw3 committee's members
      sign exactly this shape.
- [ ] A multisig member's signature, which is how a multisig committee signs: `keys add
      com --multisig flex,node0 --multisig-threshold 2`, fund `com`, write a
      `tx bank send com ... --generate-only` to `tx.json`, sign it with
      `tx sign tx.json --multisig com --from flex --sign-mode amino-json
      --output-document flex.json` (and again `--from node0`), then
      `tx multisign tx.json com flex.json node0.json` and `tx broadcast`. The review
      must show `com` as the sender, since it is not the device's own address, and the
      combined transaction must land.
- [ ] Two deliberately large proposals. The app buffers 16 kB of sign document
      (`FLASH_BUFFER_SIZE`): a `gov submit-proposal` with a 20 kB summary and an
      explicit `--gas 400000` (the chain caps a summary at 10,200 characters, so a
      simulated `--gas auto` never reaches the device) must be refused with `0x6988`,
      "transaction data exceeds the device's internal buffer capacity", before any
      review, and the app must stay up. A 6 kB summary must review to its last page and
      land. Truncated or partial display is a bug; stop and file it.
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
