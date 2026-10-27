#!/usr/bin/env bash
# Build one installer per sideloadable device in Ledger's builder image, with every
# device SDK at the tag SDK_TAG names. The tag sets the API level, so the installers
# load on the firmware at that level, and release_notes.py reads it back from them.
set -euo pipefail

: "${BUILDER:?the builder image, pinned by digest}"
: "${SDK_TAG:?the ledger-secure-sdk tag to build at}"
out=${1:?usage: build_installers.sh OUT_DIR}
mkdir -p "$out"

# SDK variable in the image, installer the build writes, its name in the release.
build() {
  docker run --rm -v "$PWD":/app -w /app/app -e SDK_VAR="$1" -e SDK_TAG "$BUILDER" bash -euo pipefail -c '
    sdk=${!SDK_VAR}
    git -C "$sdk" fetch -q --tags origin
    git -C "$sdk" checkout -q "$SDK_TAG"
    make clean >/dev/null
    BOLOS_SDK=$sdk make -j"$(nproc)"
  '
  cp "app/pkg/$2" "$out/$3"
}

build NANOSP_SDK installer_s2.sh     installer_nanos_plus.sh
build STAX_SDK   installer_stax.sh   installer_stax.sh
build FLEX_SDK   installer_flex.sh   installer_flex.sh
build APEX_P_SDK installer_apex_p.sh installer_apex_p.sh
