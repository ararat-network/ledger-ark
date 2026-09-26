/*******************************************************************************
 *   (c) 2026 Zondax AG
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

#include <gmock/gmock.h>

#include "bech32.h"
#include "chain_config.h"
#include <cstring>
#include <string>

// checkChainConfig() is the single decision point behind both address flows:
// GET_ADDR_SECP256K1 and the sign-init path (apdu_handler.c) both turn
// UNSUPPORTED into APDU_CODE_CHAIN_CONFIG_NOT_SUPPORTED. Everything asserted
// here therefore holds on both.
namespace {

address_encoding_e check(uint32_t coinType, const char *hrp) {
  return checkChainConfig(0x80000000u | coinType, hrp,
                          static_cast<uint8_t>(strlen(hrp)));
}

}  // namespace

// The app serves exactly two derivation domains: 330' (the Terra coin type
// Ark continues) and 118' (pre-2019 legacy Terra wallets). Both derive
// standard sha256/ripemd160 bech32 addresses.
TEST(ChainConfig, ArkHrpResolvesOnBothSupportedPaths) {
  EXPECT_EQ(check(330, "ark"), BECH32_STANDARD);
  EXPECT_EQ(check(118, "ark"), BECH32_STANDARD);
}

// There is no HRP table: any well-formed HRP is accepted on a supported path,
// so wallets that re-encode the same key under another prefix keep working.
TEST(ChainConfig, AnyWellFormedHrpResolvesOnSupportedPaths) {
  for (const char *hrp : {"terra", "osmo", "arkvaloper", "c4e", "e-money"}) {
    EXPECT_EQ(check(330, hrp), BECH32_STANDARD) << hrp;
    EXPECT_EQ(check(118, hrp), BECH32_STANDARD) << hrp;
  }
}

TEST(ChainConfig, AnyOtherCoinTypeIsRefused) {
  for (const uint32_t coinType : {0u, 1u, 60u, 119u, 329u, 331u, 529u}) {
    EXPECT_EQ(check(coinType, "ark"), UNSUPPORTED) << coinType;
  }
}

// A non-hardened coin type is never accepted: the supported paths are
// compared against `0x80000000 | path`.
TEST(ChainConfig, NonHardenedPathIsRefused) {
  EXPECT_EQ(checkChainConfig(330, "ark", 3), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(118, "ark", 3), UNSUPPORTED);
}

// ---------------------------------------------------------------------------
// HRP well-formedness
//
// The caller declares the HRP length out of band (an APDU byte), so the bytes
// in between are attacker-chosen and are not necessarily a C string.
// Everything downstream of this function measures the HRP with strlen():
// bech32EncodeFromBytes() does. That mismatch is the whole problem, so the
// bytes are validated here, at the single decision point, before the encoder
// sees them.
// ---------------------------------------------------------------------------

// The load-bearing case. "ark\0X" declared as 5 bytes would pass a naive
// length check, and the encoder would then truncate it to "ark": the device
// would display an address under a different HRP than the one it validated.
TEST(ChainConfig, EmbeddedNulIsRefused) {
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "ark\0X", 5), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 118u, "ark\0X", 5), UNSUPPORTED);
}

// A trailing NUL inside the declared length is the same bypass with the same
// payload, and is refused for the same reason.
TEST(ChainConfig, TrailingNulInsideTheDeclaredLengthIsRefused) {
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "ark\0", 4), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 118u, "terra\0", 6), UNSUPPORTED);
}

// Pins the downstream behaviour the check above compensates for. If zxlib ever
// grows a length-aware encoder this test is what says the gate can be revisited.
TEST(ChainConfig, Bech32EncoderMeasuresTheHrpWithStrlen) {
  const uint8_t payload[20] = {0};
  char truncated[128] = {0};
  char plain[128] = {0};

  ASSERT_EQ(bech32EncodeFromBytes(truncated, sizeof(truncated), "ark\0X", payload,
                                  sizeof(payload), 1, BECH32_ENCODING_BECH32),
            zxerr_ok);
  ASSERT_EQ(bech32EncodeFromBytes(plain, sizeof(plain), "ark", payload,
                                  sizeof(payload), 1, BECH32_ENCODING_BECH32),
            zxerr_ok);

  EXPECT_STREQ(truncated, plain);
}

// bech32 HRPs are lowercase. Uppercase was already refused by bech32_encode(),
// but only after the request had been declared supported -- so the device
// reported "invalid data" instead of "chain config not supported", and the
// rejection depended on a library two layers down.
TEST(ChainConfig, UppercaseHrpIsRefused) {
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "ARK", 3), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "Ark", 3), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 118u, "TERRA", 5), UNSUPPORTED);
}

// bech32 restricts the HRP to printable ASCII, [33, 126]. Space and DEL sit
// just outside it on either side; 0x80 and 0xFF cover the high half.
TEST(ChainConfig, NonPrintableOrNonAsciiHrpIsRefused) {
  for (const char *hrp : {"ar\x01k", "ar k", "ar\x7fk", "ar\x80k", "ar\xffk",
                          "ar\tk", "ar\nk"}) {
    EXPECT_EQ(checkChainConfig(0x80000000u | 330u, hrp, 4), UNSUPPORTED) << hrp;
  }
}

// The NUL does not have to sit after the expected prefix to be a problem:
// whatever the encoder finds before it is what the user is shown.
TEST(ChainConfig, LeadingNulIsRefused) {
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "\0ark", 4), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "\0", 1), UNSUPPORTED);
}

// hrpLen is a uint8_t and so is the scan index. The callers cap the length at
// MAX_BECH32_HRP_LEN (83), but the function has to terminate on any value a
// uint8_t can hold, including 255.
TEST(ChainConfig, ScanTerminatesAtTheMaximumDeclaredLength) {
  const std::string longHrp(255, 'a');
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, longHrp.c_str(), 255),
            BECH32_STANDARD);

  std::string longHrpWithNul(255, 'a');
  longHrpWithNul[254] = '\0';
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, longHrpWithNul.c_str(), 255),
            UNSUPPORTED);
}

TEST(ChainConfig, NullOrEmptyHrpIsRefused) {
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, nullptr, 0), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, nullptr, 3), UNSUPPORTED);
  EXPECT_EQ(checkChainConfig(0x80000000u | 330u, "ark", 0), UNSUPPORTED);
}
