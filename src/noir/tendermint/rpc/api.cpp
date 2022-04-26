// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tendermint/rpc/api.h>

namespace noir::tendermint::rpc {

using namespace fc;

fc::variant api::broadcast_tx_async(const variant& req) {
  auto tx = req.get_object()["tx"].as_string();
  auto d = base64_decode(tx);
  std::vector<char> raw_tx(d.begin(), d.end());
  auto result = mempool_->broadcast_tx_async(raw_tx);
  variant res;
  to_variant(result, res);
  return res;
}

fc::variant api::broadcast_tx_sync(const variant& req) {
  auto enc_tx = req.get_object()["tx"].as_string();
  auto d = base64_decode(enc_tx);
  std::vector<char> raw_tx(d.begin(), d.end());
  auto result = mempool_->broadcast_tx_sync(raw_tx);
  variant res;
  to_variant(result, res);
  return res;
}

fc::variant api::broadcast_tx_commit(const variant& req) {
  //      auto enc_tx = req.get_object()["tx"].as_string();
  //      auto d = fc::base64_decode(enc_tx);
  //      std::vector<char> raw_tx(d.begin(), d.end());
  //      auto result = mempool_->broadcast_tx_commit(raw_tx);
  //      variant res;
  //      to_variant(result, res);
  //      return res;
  check(false, "not implemented yet");
  return fc::variant(nullptr);
}

fc::variant api::unconfirmed_txs(const variant& req) {
  auto limit = req.get_object()["limit"].as<uint32_t>();
  auto result = mempool_->unconfirmed_txs(limit);
  variant res;
  to_variant(result, res);
  return res;
}

fc::variant api::num_unconfirmed_txs(const variant& req) {
  auto result = mempool_->num_unconfirmed_txs();
  variant res;
  to_variant(result, res);
  return res;
}

fc::variant api::check_tx(const variant& req) {
  auto enc_tx = req.get_object()["tx"].as_string();
  auto d = base64_decode(enc_tx);
  std::vector<char> raw_tx(d.begin(), d.end());
  auto result = mempool_->check_tx(raw_tx);
  variant res;
  to_variant(result, res);
  return res;
}

} // namespace noir::tendermint::rpc
