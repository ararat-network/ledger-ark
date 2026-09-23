/*******************************************************************************
 *   (c) 2018 - 2023 Zondax AG
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/
#include "chain_config.h"

#include <zxmacros.h>

address_encoding_e checkChainConfig(uint32_t path, const char *hrp,
                                    uint8_t hrpLen) {
  if (hrp == NULL || hrpLen == 0) {
    return UNSUPPORTED;
  }

  // Reject anything that is not a well-formed bech32 HRP. The load-bearing
  // case is the embedded NUL: bech32EncodeFromBytes() measures the HRP with
  // strlen(), so a declared "ark\0X" (hrpLen 5) would silently encode under
  // the truncated "ark" while the caller believes a longer HRP was validated.
  // Non-printable, non-ASCII and uppercase bytes are rejected here too:
  // bech32_encode() already refuses them, but only after the chain has been
  // declared supported, so catching them at the single decision point keeps
  // the status word accurate.
  for (uint8_t i = 0; i < hrpLen; i++) {
    const uint8_t ch = (uint8_t)hrp[i];
    if (ch < 33 || ch > 126 || (ch >= 'A' && ch <= 'Z')) {
      return UNSUPPORTED;
    }
  }

  // Any well-formed HRP derives a standard Cosmos-style address on the Terra
  // coin type Ark continues, or on the legacy Cosmos coin type that pre-2019
  // Terra wallets used.
  if (path == HDPATH_1_DEFAULT || path == HDPATH_1_LEGACY) {
    return BECH32_COSMOS;
  }

  return UNSUPPORTED;
}
