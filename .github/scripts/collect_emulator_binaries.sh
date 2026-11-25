#!/usr/bin/env bash
# Copy the ELFs the emulator build left in app/output into OUT_DIR under their release
# names, ark_zemu_<model>_v<version>.elf. <model> is Zemu's name for the device, and the
# table is the one in tests_zemu/tests/common.ts. Run it after the Zemu suite has passed
# on these files, so a release attaches the bytes the tests ran against.
set -euo pipefail

out=${1:?usage: collect_emulator_binaries.sh OUT_DIR}
mkdir -p "$out"

# shellcheck source=app/Makefile.version
. app/Makefile.version
version="v${APPVERSION_M}.${APPVERSION_N}.${APPVERSION_P}"

# ELF the build writes, Zemu's model name.
collected=0
collect() {
  cp "app/output/$1" "$out/ark_zemu_$2_$version.elf"
  collected=$((collected + 1))
}

collect app_x.elf      nanox
collect app_s2.elf     nanosp
collect app_stax.elf   stax
collect app_flex.elf   flex
collect app_apex_p.elf apex_p

# A device the build gains must be added above, not left out of the release in silence.
built=(app/output/app_*.elf)
if [ "${#built[@]}" -ne "$collected" ]; then
  echo "::error::app/output holds ${#built[@]} device ELFs and $collected are collected: ${built[*]}"
  exit 1
fi
