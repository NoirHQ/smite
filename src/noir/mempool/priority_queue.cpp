// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/mempool/priority_queue.h>
#include <noir/common/scope_exit.h>
#include <mutex>

namespace noir::mempool {

auto TxPriorityQueue::get_evictable_txs(int64_t priority, int64_t tx_size, int64_t total_size, int64_t cap) -> std::vector<std::shared_ptr<WrappedTx>> {
  std::shared_lock g{mtx};

  std::vector<std::shared_ptr<WrappedTx>> to_evict;
  auto curr_size = total_size;
  auto& heap = txs.get<1>();
  for (auto& tx : heap) {
    if (tx->priority >= priority) {
      break;
    }

    to_evict.push_back(tx);
    curr_size -= tx->size();

    if ((curr_size + tx_size) <= cap) {
      return to_evict;
    }
  }
  return {};
}

auto TxPriorityQueue::num_txs() -> size_t {
  std::shared_lock g{mtx};
  return txs.size();
}

void TxPriorityQueue::remove_tx(const std::shared_ptr<WrappedTx>& tx) {
  std::unique_lock g{mtx};
  txs.erase(txs.find(tx.get()));
}

void TxPriorityQueue::push_tx(const std::shared_ptr<WrappedTx>& tx) {
  std::unique_lock g{mtx};
  txs.insert(tx);
}

void TxPriorityQueue::push_tx(std::shared_ptr<WrappedTx>&& tx) {
  std::unique_lock g{mtx};
  txs.insert(std::move(tx));
}

auto TxPriorityQueue::pop_tx() -> std::shared_ptr<WrappedTx> {
  std::unique_lock g{mtx};

  auto& heap = txs.get<1>();
  if (!heap.size()) {
    return nullptr;
  }
  // erase() doesn't work with reverse iterator
  auto rbegin = --heap.end();
  auto _ = make_scope_exit([&]() {
    heap.erase(rbegin);
  });
  return *rbegin;
}

} // namespace noir::mempool
