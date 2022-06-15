// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/mempool/tx.h>
#include <range/v3/view/enumerate.hpp>
#include <mutex>

namespace noir::mempool {

auto WrappedTx::size() const -> size_t {
  return tx.size();
}

auto WrappedTx::key() const -> types::TxKey {
  return tx.key();
}

auto WrappedTx::ptr() -> WrappedTx* {
  return this;
}

auto TxStore::size() -> size_t {
  std::shared_lock g{mtx};
  return txs.size();
}

auto TxStore::get_all_txs() -> std::vector<std::shared_ptr<WrappedTx>> {
  std::shared_lock g{mtx};
  auto wtxs = std::vector<std::shared_ptr<WrappedTx>>(txs.size());
  for (auto [i, wtx] : ranges::views::enumerate(txs.get<TxStore::by_key>())) {
    wtxs[i] = wtx;
  }
  return wtxs;
}

auto TxStore::get_tx_by_sender(const std::string& sender) -> std::shared_ptr<WrappedTx> {
  // original implementation doesn't insert the wrapped tx with the empty sender to sender index.
  if (sender.empty()) {
    return nullptr;
  }
  std::shared_lock g{mtx};
  auto& sender_txs = txs.get<TxStore::by_sender>();
  if (auto wtx = sender_txs.find(sender); wtx != sender_txs.end()) {
    return *wtx;
  }
  return nullptr;
}

auto TxStore::get_tx_by_hash(const types::TxKey& hash) -> std::shared_ptr<WrappedTx> {
  std::shared_lock g{mtx};
  auto& hash_txs = txs.get<TxStore::by_key>();
  if (auto wtx = hash_txs.find(hash); wtx != hash_txs.end()) {
    return *wtx;
  }
  return nullptr;
}

auto TxStore::is_tx_removed(const types::TxKey& hash) -> bool {
  std::shared_lock g{mtx};
  auto& hash_txs = txs.get<TxStore::by_key>();
  if (auto wtx = hash_txs.find(hash); wtx != hash_txs.end()) {
    return (*wtx)->removed;
  }
  return false;
}

void TxStore::set_tx(const std::shared_ptr<WrappedTx>& wtx) {
  std::unique_lock g{mtx};
  txs.insert(wtx);
}

void TxStore::remove_tx(const std::shared_ptr<WrappedTx>& wtx) {
  std::unique_lock g{mtx};
  txs.erase(wtx->tx.key());
  wtx->removed = true;
}

auto TxStore::tx_has_peer(const types::TxKey& hash, uint16_t peer_id) -> bool {
  std::shared_lock g{mtx};

  auto& hash_txs = txs.get<TxStore::by_key>();
  auto wtx = hash_txs.find(hash);
  if (wtx == hash_txs.end()) {
    return false;
  }
  return (*wtx)->peers.contains(peer_id);
}

auto TxStore::get_or_set_peer_by_tx_hash(const types::TxKey& hash, uint16_t peer_id) -> std::pair<std::shared_ptr<WrappedTx>, bool> {
  std::unique_lock g{mtx};

  auto& hash_txs = txs.get<TxStore::by_key>();
  auto wtx = hash_txs.find(hash);
  if (wtx == hash_txs.end()) {
    return {nullptr, false};
  }

  if ((*wtx)->peers.contains(peer_id)) {
    return {*wtx, true};
  }

  (*wtx)->peers.insert(peer_id);
  return {*wtx, false};
}

auto WrappedTxList::size() -> size_t {
  std::shared_lock g{mtx};
  return txs.size();
}

void WrappedTxList::reset() {
  std::unique_lock g{mtx};
  txs.clear();
}

void WrappedTxList::insert(const std::shared_ptr<WrappedTx>& wtx) {
  std::unique_lock g{mtx};

  auto i = std::find_if(txs.begin(), txs.end(), [&](const auto& v) {
    return less(v, wtx);
  });

  txs.insert(i, wtx);
}

void WrappedTxList::insert(std::shared_ptr<WrappedTx>&& wtx) {
  std::unique_lock g{mtx};

  auto i = std::find_if(txs.begin(), txs.end(), [&](const auto& v) {
    return less(v, wtx);
  });

  txs.insert(i, std::move(wtx));
}

void WrappedTxList::remove(const std::shared_ptr<WrappedTx>& wtx) {
  std::unique_lock g{mtx};

  auto i = std::find_if(txs.begin(), txs.end(), [&](const auto& v) {
    return less(v, wtx);
  });

  while (i != txs.end()) {
    if ((*i).get() == wtx.get()) {
      txs.erase(i);
      return;
    }
    i++;
  }
}

} // namespace noir::mempool
