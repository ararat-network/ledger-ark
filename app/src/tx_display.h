/*******************************************************************************
 *   (c) 2018, 2019 Zondax GmbH
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

#pragma once

#include "coin.h"
#include "parser_impl.h"
#include "parser_txdef.h"
#include <common/parser_common.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The most items a review may have. A document that needs more is refused, not
// shown in part.
//
// The number comes from zxlib's viewfunc_getItem_t as it was through zxlib 51,
// where displayIdx was int8_t: an index above 127 arrived negative, and the
// Nano review asks once more at index == numItems as its end-of-list sentinel,
// so 127 was the largest count whose every index still reached the callback.
// Since zxlib 56 the index is uint8_t and the view reserves only 0xFF
// (VIEW_INTRO_DISPLAY_IDX), so the screen could now reach further. The ceiling
// is kept where it was: raising it changes which documents the app signs, and
// that is a decision of its own, not a side effect of a library update.
#define MAX_REVIEW_ITEMS 127

typedef enum {
  root_item_chain_id = 0,
  root_item_account_number,
  root_item_sequence,
  root_item_msgs,
  root_item_memo,
  root_item_fee,
  root_item_tip,
  root_item_timeout_height,
  root_item_unordered,
  root_item_timeout_timestamp,
} root_item_e;

parser_error_t
tx_is_expert_mode_or_not_default_chainid(bool *expert_or_default);

const char *get_required_root_item(root_item_e i);

parser_error_t tx_display_query(uint16_t displayIdx, char *outKey,
                                uint16_t outKeyLen,
                                uint16_t *ret_value_token_index);

parser_error_t tx_display_numItems(uint8_t *num_items);

parser_error_t tx_display_make_friendly();

parser_error_t tx_display_translation(char *dst, uint16_t dstLen, char *src,
                                      uint16_t srcLen);
//---------------------------------------------

#ifdef __cplusplus
}
#endif
