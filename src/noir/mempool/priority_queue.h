// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/tx.h>
#include <noir/mempool/tx.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <shared_mutex>

namespace noir::mempool {

template<typename... Indices>
class TxPriorityQueue {
public:
  auto get_evictable_txs(int64_t priority, int64_t tx_size, int64_t total_size, int64_t cap) -> std::vector<std::shared_ptr<WrappedTx>> {
    std::shared_lock g{mtx};

    std::vector<std::shared_ptr<WrappedTx>> to_evict;
    auto curr_size = total_size;
    auto& heap = txs.template get<TxPriorityQueue::by_priority>();
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

  auto num_txs() -> size_t {
    std::shared_lock g{mtx};
    return txs.size();
  }

  void remove_tx(const std::shared_ptr<WrappedTx>& tx) {
    std::unique_lock g{mtx};
    txs.erase(txs.find(tx.get()));
  }

  void push_tx(const std::shared_ptr<WrappedTx>& tx) {
    std::unique_lock g{mtx};
    txs.insert(tx);
  }

  void push_tx(std::shared_ptr<WrappedTx>&& tx) {
    std::unique_lock g{mtx};
    txs.insert(std::move(tx));
  }

  auto pop_tx() -> std::shared_ptr<WrappedTx> {
    std::unique_lock g{mtx};

    auto& heap = txs.template get<TxPriorityQueue::by_priority>();
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

  // XXX: original implementation includes heap interfaces, but here boost::multi_index handles them

  struct by_priority;
  // struct by_height;
  // struct by_timestamp;

private:
  struct less {
    bool operator()(const WrappedTx& lhs, const WrappedTx& rhs) {
      if (lhs.priority == rhs.priority) {
        return lhs.timestamp > rhs.timestamp;
      }
      return lhs.priority < rhs.priority;
    }
  };

  template<typename T>
  using tag = boost::multi_index::tag<T>;

  // clang-format off
  using Txs = boost::multi_index_container<
    std::shared_ptr<WrappedTx>,
    boost::multi_index::indexed_by<
      // XXX: not sure this is a good way for setting a primary key
      boost::multi_index::ordered_unique<boost::multi_index::key<&WrappedTx::ptr>>,
      //
      boost::multi_index::ordered_non_unique<tag<by_priority>, boost::multi_index::identity<WrappedTx>, TxPriorityQueue::less>,
      // boost::multi_index::ordered_non_unique<tag<by_height>, boost::multi_index::key<&WrappedTx::height>>,
      // boost::multi_index::ordered_non_unique<tag<by_timestamp>, boost::multi_index::key<&WrappedTx::timestamp>>,
      Indices...
    >
  >;
  // clang-format on

  std::shared_mutex mtx;

public:
  // FIXME: temporarily public for testing
  Txs txs;
};

} // namespace noir::mempool
