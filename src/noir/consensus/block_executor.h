// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <noir/consensus/app_connection.h>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/types.h>

#include <utility>

namespace noir::consensus {

/**
 * Provides functions for executing a block and updates state and mempool
 */
struct block_executor {

  db_store store_;

  block_store block_store_;

  std::shared_ptr<app_connection> proxyApp_;

  // eventBus // todo - we may not need to handle events for tendermint but only for app?

  // mempool // todo

  // evidence tool // todo

  std::map<std::string, bool> cache; // storing verification result for a single height

  block_executor(
    db_store& new_store, std::shared_ptr<app_connection> new_proxyApp, /* mempool */ block_store& new_block_store)
    : store_(new_store), proxyApp_(std::move(new_proxyApp)), block_store_(new_block_store) {}

  static std::unique_ptr<block_executor> new_block_executor(db_store& new_store,
    const std::shared_ptr<app_connection>& new_proxyApp, /* mempool */ block_store& new_block_store) {
    auto res = std::make_unique<block_executor>(new_store, new_proxyApp, new_block_store);
    return res;
  }

  /* todo - return block, part_set */ void create_proposal_block() {
    // todo
  }

  bool validate_block(state& state_, block& block_) {
    auto hash = block_.get_hash();
    if (cache.find(to_hex(hash)) != cache.end())
      return true;

    // validate block
    // todo - implement; it's very big (state/validation.go)

    // check evidence // todo

    cache[to_hex(hash)] = true;
    return true;
  }

  std::optional<state> apply_block(state& state_, p2p::block_id block_id_, block& block_) {
    if (!validate_block(state_, block_)) {
      elog("apply block failed: invalid block");
      return {};
    }

    auto start_time = get_time();
    auto abci_responses_ = exec_block_on_proxy_app(proxyApp_, block_, store_, state_.initial_height);
    auto end_time = get_time();
    if (!abci_responses_.has_value()) {
      elog("apply block failed: proxy app");
      return {};
    }

    if (!store_.save_abci_responses(block_.header.height /* todo abci_response*/)) {
      return {};
    }

    auto abci_val_updates = abci_responses_->end_block.validator_updates;
    if (!validate_validator_update(abci_val_updates, state_.consensus_params_.validator)) {
      elog("apply block failed: error in validator updates");
      return {};
    }

    // todo - implement rest
  }

  void extend_vote() {}

  void verify_vote_extension() {}

  void commit() {}

  std::optional<abci_responses> exec_block_on_proxy_app(
    std::shared_ptr<app_connection> proxyAppConn, block& block_, db_store& db_store, int64_t initial_height) {
    uint valid_txs = 0, invalid_txs = 0, tx_index = 0;
    abci_responses abci_responses_;
    std::vector<response_deliver_tx> dtxs;
    dtxs.resize(block_.data.txs.size());
    abci_responses_.deliver_txs = dtxs;

    auto proxy_cb = [](/* req, res - todo */) {
      // todo - implement
    };
    // proxyAppConn.set_response_callback(proxy_cb); // todo

    // todo - implement the rest
  }

  void get_begin_block_validator_info() {}

  bool validate_validator_update(std::vector<validator_update> abci_updates, validator_params params) {
    for (auto val_update : abci_updates) {
      if (val_update.power < 0) {
        elog("voting power can't be negative");
        return false;
      } else if (val_update.power == 0) {
        continue;
      }
      // todo - implement rest
    }
    return true;
  }

  std::optional<state> update_state(state& state_, p2p::block_id block_id_, block_header header_) {}

  uint64_t prune_blocks(int64_t retain_height) {
    auto base = block_store_.base();
    if (retain_height <= base)
      return 0;
    uint64_t pruned;
    if (!block_store_.prune_blocks(retain_height, pruned)) {
      elog("filed to prune block store");
      return 0;
    }
    if (!store_.prune_states(retain_height)) {
      elog("filed to prune state store");
      return 0;
    }
    return pruned;
  }
};

} // namespace noir::consensus
