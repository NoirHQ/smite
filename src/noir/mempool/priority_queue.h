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
  auto get_evictable_txs(int64_t priority, int64_t tx_size, int64_t total_size, int64_t cap) -> std::vector<std::shared_ptr<WrappedTx>>;
  auto num_txs() -> size_t;
  void remove_tx(const std::shared_ptr<WrappedTx>& tx);
  void push_tx(const std::shared_ptr<WrappedTx>& tx);
  void push_tx(std::shared_ptr<WrappedTx>&& tx);
  auto pop_tx() -> std::shared_ptr<WrappedTx>;

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
