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

#include "app_mode.h"
#include "coin.h"
#include "common.h"
#include "common/parser.h"
#include <string>
#include <vector>

// An unordered transaction (Cosmos SDK 0.53 and later) is bounded by an expiry
// instead of a sequence. Its amino sign document carries "unordered":true and
// "timeout_timestamp", and is signed at sequence zero.
//
// The three documents below are sign bytes, not hand-written JSON: each was
// built by ark-sdk's makeAminoSignDoc and aminoSignDocBytes, and is
// byte-identical to what the chain's x/tx aminojson handler (Cosmos SDK
// v0.54.5) returns from GetSignBytes for the same transaction.
namespace {

// MsgSend, unordered, on the default chain, expiring on a whole second.
const char *const kUnordered =
    R"({"account_number":"42","chain_id":"ark-1","fee":{"amount":[{"amount":"2000000000000000","denom":"anoah"}],"gas":"200000"},"memo":"","msgs":[{"type":"cosmos-sdk/MsgSend","value":{"amount":[{"amount":"1500000000000000000","denom":"anoah"}],"from_address":"ark102p6s52n97e7zcmxc49ufwx04lntre5ktqkkzp","to_address":"ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel8nq"}}],"sequence":"0","timeout_timestamp":"2026-11-25T12:10:00Z","unordered":true})";

// The same send on another chain, with a memo, a timeout height, and an expiry
// that carries nanoseconds.
const char *const kUnorderedOtherChain =
    R"({"account_number":"7","chain_id":"localark","fee":{"amount":[{"amount":"2000000000000000","denom":"anoah"}],"gas":"200000"},"memo":"rent","msgs":[{"type":"cosmos-sdk/MsgSend","value":{"amount":[{"amount":"1500000000000000000","denom":"anoah"}],"from_address":"ark102p6s52n97e7zcmxc49ufwx04lntre5ktqkkzp","to_address":"ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel8nq"}}],"sequence":"0","timeout_height":"9000","timeout_timestamp":"2026-11-25T12:10:00.123456Z","unordered":true})";

// An ordered transaction may set a timeout timestamp too: the chain then
// refuses it after that time, and still orders it by its sequence.
const char *const kOrderedWithTimeoutTimestamp =
    R"({"account_number":"42","chain_id":"ark-1","fee":{"amount":[{"amount":"2000000000000000","denom":"anoah"}],"gas":"200000"},"memo":"","msgs":[{"type":"cosmos-sdk/MsgSend","value":{"amount":[{"amount":"1500000000000000000","denom":"anoah"}],"from_address":"ark102p6s52n97e7zcmxc49ufwx04lntre5ktqkkzp","to_address":"ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel8nq"}}],"sequence":"5","timeout_timestamp":"2026-11-25T12:10:00Z"})";

// kUnordered with the sequence value and everything after it replaced, for the
// documents the chain cannot build. `sequence` is the JSON value as written,
// quotes included; `tail` is the members that follow it.
std::string doc(const std::string &sequence, const std::string &tail) {
  return R"({"account_number":"42","chain_id":"ark-1","fee":{"amount":[{"amount":"2000000000000000","denom":"anoah"}],"gas":"200000"},"memo":"","msgs":[{"type":"cosmos-sdk/MsgSend","value":{"amount":[{"amount":"1500000000000000000","denom":"anoah"}],"from_address":"ark102p6s52n97e7zcmxc49ufwx04lntre5ktqkkzp","to_address":"ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel8nq"}}],"sequence":)" +
         sequence + tail + "}";
}

// An unordered document expiring at the given timeout_timestamp value, written
// as it appears in the JSON, quotes included.
std::string expiring(const std::string &timeout_timestamp) {
  return doc(R"("0")", R"(,"timeout_timestamp":)" + timeout_timestamp +
                           R"(,"unordered":true)");
}

parser_error_t validate(const std::string &tx) {
  parser_context_t ctx;
  parser_tx_t tx_obj;
  memset(&tx_obj, 0, sizeof(tx_obj));
  tx_obj.tx_type = tx_json;

  const auto err =
      parser_parse(&ctx, (const uint8_t *)tx.c_str(), tx.size(), &tx_obj);
  if (err != parser_ok) {
    return err;
  }
  return parser_validate(&ctx);
}

std::vector<std::string> render(const std::string &tx, bool expert) {
  app_mode_set_expert(expert);

  parser_context_t ctx;
  parser_tx_t tx_obj;
  memset(&tx_obj, 0, sizeof(tx_obj));
  tx_obj.tx_type = tx_json;

  auto err =
      parser_parse(&ctx, (const uint8_t *)tx.c_str(), tx.size(), &tx_obj);
  if (err != parser_ok) {
    return {parser_getErrorDescription(err)};
  }
  err = parser_validate(&ctx);
  if (err != parser_ok) {
    return {parser_getErrorDescription(err)};
  }
  return dumpUI(&ctx, 40, 40);
}

} // namespace

// The helper rebuilds the real document exactly, so every refusal below
// differs from accepted sign bytes only where the test says it does.
TEST(Unordered, TheHelperRebuildsTheRealDocument) {
  EXPECT_EQ(expiring(R"("2026-11-25T12:10:00Z")"), kUnordered);
}

TEST(Unordered, BothFieldsAreShownOutsideExpertModeOnTheDefaultChain) {
  const std::vector<std::string> expected = {
      "0 | Type : Send",
      "1 | Amount : 1.500000 NOAH",
      "2 | From [1/2] : ark102p6s52n97e7zcmxc49ufwx04lntre5ktqk",
      "2 | From [2/2] : kzp",
      "3 | To [1/2] : ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel",
      "3 | To [2/2] : 8nq",
      "4 | Fee : 0.002000 NOAH",
      "5 | Unordered : true",
      "6 | Timeout Timestamp : 2026-11-25T12:10:00Z",
  };
  EXPECT_EQ(render(kUnordered, false), expected);
}

// Expert mode adds the fields it always adds. The sequence an unordered
// transaction is signed at is zero, and is shown as that.
TEST(Unordered, BothFieldsAreShownInExpertMode) {
  const std::vector<std::string> expected = {
      "0 | Chain ID : ark-1",
      "1 | Account : 42",
      "2 | Sequence : 0",
      "3 | Type : Send",
      "4 | Amount : 1500000000000000000 anoah",
      "5 | From [1/2] : ark102p6s52n97e7zcmxc49ufwx04lntre5ktqk",
      "5 | From [2/2] : kzp",
      "6 | To [1/2] : ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel",
      "6 | To [2/2] : 8nq",
      "7 | Fee : 2000000000000000 anoah",
      "8 | Gas : 200000",
      "9 | Unordered : true",
      "10 | Timeout Timestamp : 2026-11-25T12:10:00Z",
  };
  EXPECT_EQ(render(kUnordered, true), expected);
}

// Another chain ID switches the review to its expert layout whatever the
// setting. The expiry keeps the nanoseconds it was signed with, and follows
// the timeout height.
TEST(Unordered, BothFieldsAreShownOnAnotherChain) {
  const std::vector<std::string> expected = {
      "0 | Chain ID : localark",
      "1 | Account : 7",
      "2 | Sequence : 0",
      "3 | Type : Send",
      "4 | Amount : 1500000000000000000 anoah",
      "5 | From [1/2] : ark102p6s52n97e7zcmxc49ufwx04lntre5ktqk",
      "5 | From [2/2] : kzp",
      "6 | To [1/2] : ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel",
      "6 | To [2/2] : 8nq",
      "7 | Memo : rent",
      "8 | Fee : 2000000000000000 anoah",
      "9 | Gas : 200000",
      "10 | Timeout Height : 9000",
      "11 | Unordered : true",
      "12 | Timeout Timestamp : 2026-11-25T12:10:00.123456Z",
  };
  EXPECT_EQ(render(kUnorderedOtherChain, false), expected);
}

TEST(Unordered, TimeoutTimestampOfAnOrderedTransactionIsShown) {
  const std::vector<std::string> expected = {
      "0 | Type : Send",
      "1 | Amount : 1.500000 NOAH",
      "2 | From [1/2] : ark102p6s52n97e7zcmxc49ufwx04lntre5ktqk",
      "2 | From [2/2] : kzp",
      "3 | To [1/2] : ark1lw7jrngdgm88ruz8v7k6s70gssu6vryufel",
      "3 | To [2/2] : 8nq",
      "4 | Fee : 0.002000 NOAH",
      "5 | Timeout Timestamp : 2026-11-25T12:10:00Z",
  };
  EXPECT_EQ(render(kOrderedWithTimeoutTimestamp, false), expected);
}

// The chain omits unordered when it is false, so the key never appears with
// any value but true.
TEST(Unordered, FalseIsRefused) {
  EXPECT_EQ(validate(doc(R"("5")", R"(,"unordered":false)")),
            parser_json_unordered_not_true);
  EXPECT_EQ(
      validate(doc(
          R"("0")",
          R"(,"timeout_timestamp":"2026-11-25T12:10:00Z","unordered":false)")),
      parser_json_unordered_not_true);
}

// A bool is written bare. Anything else under the key is not the chain's
// document, whatever it would look like on the screen.
TEST(Unordered, AnythingButTheBooleanTrueIsRefused) {
  for (const char *value : {R"("true")", "1", "null", "TRUE", "truee",
                            R"({"value":true})", "[true]", R"("")"}) {
    EXPECT_EQ(
        validate(doc(
            R"("0")",
            std::string(
                R"(,"timeout_timestamp":"2026-11-25T12:10:00Z","unordered":)") +
                value)),
        parser_json_unordered_not_true)
        << value;
  }
}

TEST(Unordered, WithoutATimeoutTimestampIsRefused) {
  EXPECT_EQ(validate(doc(R"("0")", R"(,"unordered":true)")),
            parser_json_unordered_missing_timeout);
  // A timeout height is not an expiry the chain accepts in its place.
  EXPECT_EQ(
      validate(doc(R"("0")", R"(,"timeout_height":"9000","unordered":true)")),
      parser_json_unordered_missing_timeout);
}

// The chain refuses an unordered transaction with a sequence. Outside expert
// mode the review hides the sequence, so the device must not sign one here.
TEST(Unordered, WithASequenceIsRefused) {
  const std::string tail =
      R"(,"timeout_timestamp":"2026-11-25T12:10:00Z","unordered":true)";

  EXPECT_EQ(validate(doc(R"("0")", tail)), parser_ok);
  for (const char *sequence :
       {R"("5")", R"("1")", R"("00")", R"("")", R"("0 ")", "0", "null"}) {
    EXPECT_EQ(validate(doc(sequence, tail)),
              parser_json_unordered_with_sequence)
        << sequence;
  }
}

TEST(Unordered, TimeoutTimestampAsTheChainWritesItIsAccepted) {
  for (const char *value : {
           R"("2026-11-25T12:10:00Z")",
           R"("2026-11-25T12:10:00.5Z")",
           R"("2026-11-25T12:10:00.123456Z")",
           R"("2026-11-25T12:10:00.000000001Z")",
           R"("2026-11-25T12:10:00.999999999Z")",
           R"("1970-01-01T00:00:00Z")",
           R"("9999-12-31T23:59:59Z")",
       }) {
    EXPECT_EQ(validate(expiring(value)), parser_ok) << value;
  }
}

// One spelling per instant, the one Go's RFC 3339 formatting of a UTC time
// gives. The text is shown as signed, so nothing the signer would have to
// convert or guess at is let through, with or without unordered.
TEST(Unordered, AnyOtherTimeoutTimestampIsRefused) {
  for (const char *value : {
           R"("")",
           R"("tomorrow")",
           R"("2026-11-25")",
           R"("2026-11-25T12:10:00")",
           R"("2026-11-25T12:10:00z")",
           R"("2026-11-25t12:10:00Z")",
           R"("2026-11-25 12:10:00Z")",
           R"("2026-11-25T12:10:00+00:00")",
           R"("2026-11-25T21:10:00+09:00")",
           R"("2026-11-25T12:10Z")",
           R"("2026-11-25T12:10:00.Z")",
           R"("2026-11-25T12:10:00.50Z")",
           R"("2026-11-25T12:10:00.000Z")",
           R"("2026-11-25T12:10:00.1234567891Z")",
           R"("2026-11-25T12:10:00,5Z")",
           R"("2026-13-25T12:10:00Z")",
           R"("2026-00-25T12:10:00Z")",
           R"("2026-11-32T12:10:00Z")",
           R"("2026-11-00T12:10:00Z")",
           R"("2026-11-25T24:10:00Z")",
           R"("2026-11-25T12:60:00Z")",
           R"("2026-11-25T12:10:60Z")",
           R"("10000-01-01T00:00:00Z")",
           R"("-0001-01-01T00:00:00Z")",
           R"("2026-11-25T12:10:00Z ")",
           R"(" 2026-11-25T12:10:00Z")",
           "1795608600",
           "null",
           "true",
           R"({"nanos":0,"seconds":"1795608600"})",
           R"(["2026-11-25T12:10:00Z"])",
       }) {
    EXPECT_EQ(validate(expiring(value)), parser_json_invalid_timeout_timestamp)
        << value;
    EXPECT_EQ(validate(doc(R"("5")",
                           std::string(R"(,"timeout_timestamp":)") + value)),
              parser_json_invalid_timeout_timestamp)
        << "ordered, " << value;
  }
}

// What the host is told when a document is refused.
TEST(Unordered, RefusalsAreNamed) {
  EXPECT_STREQ(parser_getErrorDescription(parser_json_unordered_not_true),
               "JSON Unordered is not true");
  EXPECT_STREQ(
      parser_getErrorDescription(parser_json_unordered_missing_timeout),
      "JSON Unordered without timeout_timestamp");
  EXPECT_STREQ(parser_getErrorDescription(parser_json_unordered_with_sequence),
               "JSON Unordered with a sequence");
  EXPECT_STREQ(
      parser_getErrorDescription(parser_json_invalid_timeout_timestamp),
      "JSON Invalid timeout_timestamp");
}
