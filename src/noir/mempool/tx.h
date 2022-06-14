// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/time.h>
#include <noir/consensus/types/node_id.h>
#include <tendermint/types/mempool.h>
#include <tendermint/types/tx.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <cstdint>
#include <functional>
#include <set>
#include <shared_mutex>

namespace noir::mempool {

struct TxInfo {
  uint16_t sender_id;
  consensus::node_id sender_node_id;
};

struct WrappedTx {
  types::Tx tx;
  types::TxKey hash;
  int64_t height;
  int64_t gas_wanted;
  int64_t priority;
  std::string sender;
  tstamp timestamp;
  std::set<uint16_t> peers;
  // XXX: heap_index for priority_queue is handled by boost::multi_index
  // int heap_index;
  // gossip_el;
  bool removed;

  auto size() const -> size_t;
  auto key() const -> types::TxKey;

  auto ptr() -> WrappedTx*;
};

class TxStore {
  // clang-format off
  using Txs = boost::multi_index_container<
    std::shared_ptr<WrappedTx>,
    boost::multi_index::indexed_by<
      // FIXME: According to original implementation, following two indices should be ordered_unique,
      // but put these ordered_non_unique temporarily for testing
      boost::multi_index::ordered_non_unique<boost::multi_index::key<&WrappedTx::key>>,
      boost::multi_index::ordered_non_unique<boost::multi_index::member<WrappedTx, std::string, &WrappedTx::sender>>
    >
  >;
  // clang-format on

public:
  auto size() -> size_t;
  [[nodiscard]] auto get_all_txs() -> std::vector<std::shared_ptr<WrappedTx>>;
  auto get_tx_by_sender(const std::string& sender) -> std::shared_ptr<WrappedTx>;
  auto get_tx_by_hash(const types::TxKey& hash) -> std::shared_ptr<WrappedTx>;
  auto is_tx_removed(const types::TxKey& hash) -> bool;
  void set_tx(const std::shared_ptr<WrappedTx>& wtx);
  void remove_tx(const std::shared_ptr<WrappedTx>& wtx);
  auto tx_has_peer(const types::TxKey& hash, uint16_t peer_id) -> bool;
  auto get_or_set_peer_by_tx_hash(const types::TxKey& hash, uint16_t peer_id) -> std::pair<std::shared_ptr<WrappedTx>, bool>;

private:
  std::shared_mutex mtx;
  Txs txs;
};

class WrappedTxList {
  using LessFunc = std::function<bool(const std::shared_ptr<WrappedTx>&, const std::shared_ptr<WrappedTx>&)>;

public:
  WrappedTxList(LessFunc less): less(less) {}

  auto size() -> size_t;
  void reset();
  void insert(const std::shared_ptr<WrappedTx>& wtx);
  void insert(std::shared_ptr<WrappedTx>&& wtx);
  void remove(const std::shared_ptr<WrappedTx>& wtx);

  // FIXME: temporarily public for testing
  std::vector<std::shared_ptr<WrappedTx>> txs;

private:
  std::shared_mutex mtx;
  LessFunc less;
};

} // namespace noir::mempool
