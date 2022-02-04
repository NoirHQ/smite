// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

/**
 * Provides functions for executing a block and updates state and mempool
 */
struct block_executor {

  db_store store_;

  block_store block_store_;

  // proxyApp // todo

  // eventBus // todo - we may not need to handle events for tendermint but only for app?

  // mempool // todo

  // evidence tool // todo

  std::map<std::string, bool> cache; // storing verification result for a single height

  block_executor(db_store& new_store, /* proxyApp, mempool */ block_store& new_block_store)
    : store_(new_store), block_store_(new_block_store) {}

  static std::unique_ptr<block_executor> new_block_executor(
    db_store& new_store, /* proxyApp, mempool */ block_store& new_block_store) {
    auto res = std::make_unique<block_executor>(new_store, new_block_store);
    return res;
  }

  /* todo - return block, part_set */ void create_proposal_block() {
    // todo
  }

  void validate_block() {}

  void apply_block() {}

  void extend_vote() {}

  void verify_vote_extension() {}

  void commit() {}

  void exec_block_on_proxy_app() {}

  void get_begin_block_validator_info() {}

  void validate_validator_update() {}

  void update_state() {}

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
