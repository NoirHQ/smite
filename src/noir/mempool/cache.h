// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/concepts.h>
#include <noir/consensus/tx.h>
#include <tendermint/types/mempool.h>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <mutex>

namespace noir::mempool {

template<typename T>
concept TxCache = requires(T cache) {
  { cache.reset() } -> std::same_as<void>;
  { cache.push(consensus::tx{}) } -> std::same_as<bool>;
  { cache.remove(consensus::tx{}) } -> std::same_as<void>;
};

class LRUTxCache {
public:
  void reset();
  bool push(const consensus::tx& tx);
  void remove(const consensus::tx& tx);

  LRUTxCache() = default;
  LRUTxCache(int cache_size): size(cache_size) {}

private:
  // clang-format off
  using TxKeys = boost::multi_index_container<
    types::TxKey,
    boost::multi_index::indexed_by<
      boost::multi_index::sequenced<>,
      boost::multi_index::ordered_unique<boost::multi_index::identity<Bytes>>
    >
  >;
  // clang-format on

  std::mutex mtx;
  int size;

public:
  // FIXME: temporarily public for testing
  TxKeys tx_keys;
};

} // namespace noir::mempool
