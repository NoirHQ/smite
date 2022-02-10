// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/rlp.h>
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <eth/common/transaction.h>
#include <eth/rpc/api.h>

namespace eth::api {

using namespace noir;
using namespace noir::codec;

fc::variant api::send_raw_tx(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto& params = req.get_array();
  check(!params.empty(), "invalid parameters: not enough params to decode");
  check(params.size() == 1, "too many arguments, want at most 1");
  check(params[0].is_string(), "invalid parameters: json: cannot unmarshal");
  auto rlp = params[0].get_string();
  check(rlp.starts_with("0x"), "transaction could not be decoded: input must start with 0x");

  auto t = rlp::decode<transaction>(from_hex(rlp));
  check(tx_fee_cap == 0 || t.fee() <= tx_fee_cap, "tx fee exceeds the configured cap");
  if (!allow_unprotected_txs && !(t.v != 0 && t.v != 1 && t.v != 27 && t.v != 28)) {
    throw std::runtime_error("only replay-protected transactions allowed over RPC");
  }

  // TODO: add tx pool and return tx_hash
  std::string tx_hash;
  return fc::variant(tx_hash);
}

} // namespace eth::api