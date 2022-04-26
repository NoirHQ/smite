// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/tx.h>
#include <noir/tendermint/rpc/mempool.h>

namespace noir::tendermint::rpc {

using namespace noir;
using namespace noir::consensus;

using tx = bytes;

result_broadcast_tx mempool::broadcast_tx_async(const tx& t) {
  tx_poor_->check_tx_async(t);
  auto tx_hash = get_tx_hash(t);
  return result_broadcast_tx{.hash = tx_hash};
}

result_broadcast_tx mempool::broadcast_tx_sync(const tx& t) {
  auto r = tx_poor_->check_tx_sync(t);
  return result_broadcast_tx{.code = r.code,
    .data = r.data,
    .log = r.log,
    .codespace = r.codespace,
    .mempool_error = r.mempool_error,
    .hash = get_tx_hash(t)};
}

// TODO: not implemented
// result_broadcast_tx_commit mempool::broadcast_tx_commit(const tx& t) {
//  return result_broadcast_tx_commit{};
//}

result_unconfirmed_txs mempool::unconfirmed_txs(const uint32_t& limit_ptr) {
  auto txs = tx_poor_->reap_max_txs(limit_ptr);
  return result_unconfirmed_txs{
    .count = txs.size(), .total = tx_poor_->size(), .total_bytes = tx_poor_->size_bytes(), .txs = txs};
}

result_unconfirmed_txs mempool::num_unconfirmed_txs() {
  return result_unconfirmed_txs{
    .count = tx_poor_->size(), .total = tx_poor_->size(), .total_bytes = tx_poor_->size_bytes()};
}

response_check_tx& mempool::check_tx(const tx& t) {
  return tx_poor_->proxy_app_->check_tx_sync(consensus::request_check_tx{.tx = t});
}
} // namespace noir::tendermint::rpc
