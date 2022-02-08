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

  std::tuple<block, part_set> create_proposal_block(
    int64_t height, state& state_, commit& commit_, bytes proposer_addr, std::vector<vote>& votes) {
    auto max_bytes = state_.consensus_params_.block.max_bytes;
    auto max_gas = state_.consensus_params_.block.max_gas;

    // evidence // todo

    // Fetch a limited amount of valid txs
    auto max_data_bytes =
      max_bytes - max_overhead_for_block - max_header_bytes - max_commit_bytes(state_.validators.size());
    if (max_data_bytes < 0)
      throw std::runtime_error(fmt::format(
        "negative max_data_bytes: max_bytes={} is too small to accommodate header and last_commit", max_bytes));

    // auto txs = mempool.reap_max_bytes_max_gas(max_data_bytes, max_gas); // todo

    auto prepared_proposal =
      proxyApp_->prepare_proposal_sync(request_prepare_proposal{bytes{} /* todo - tx */, max_data_bytes, votes});
    auto new_txs = prepared_proposal.block_data;
    int tx_size{};
    for (auto tx : new_txs) {
      tx_size += tx.size();
      if (max_data_bytes < tx_size)
        throw std::runtime_error("block data exceeds max amount of allowed bytes");
    }

    auto modified_txs = prepared_proposal.block_data;

    return state_.make_block(height, modified_txs, commit_, proposer_addr);
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
    if (abci_responses_ == nullptr) {
      elog("apply block failed: proxy app");
      return {};
    }

    if (!store_.save_abci_responses(block_.header.height /* todo abci_response */)) {
      return {};
    }

    auto abci_val_updates = abci_responses_->end_block.validator_updates;
    if (!validate_validator_update(abci_val_updates, state_.consensus_params_.validator)) {
      elog("apply block failed: error in validator updates");
      return {};
    }

    auto validator_updates = validator_update::validator_updates(abci_val_updates);
    if (!validator_updates.has_value()) {
      elog("apply block failed: error in validator updates conversion");
      return {};
    }
    if (validator_updates->size() > 0)
      dlog(fmt::format("updates to validators: size={}", validator_updates->size()));

    auto new_state_ = update_state(state_, block_id_, block_.header, abci_responses_, validator_updates.value());
    if (!new_state_.has_value()) {
      elog("apply block failed: commit failed for application");
      return {};
    }

    /// directly implement commit()
    // mempool lock - todo

    // proxyApp_.commit_sync(); // todo

    ilog(fmt::format("committed state: height={}, num_txs... app_hash...", block_.header.height));

    // mempool update - todo
    bytes app_hash{}; // = res.data // todo
    int64_t retain_height{}; // = res.retain_height; // todo
    /// commit() ends

    // Update app_hash and save the state
    new_state_->app_hash = app_hash;
    if (!store_.save(new_state_.value())) {
      elog("apply block failed: save failed");
      return {};
    }

    // Prune old height
    if (retain_height > 0) {
      auto pruned = prune_blocks(retain_height);
      if (pruned > 0)
        dlog(fmt::format("pruned blocks: pruned={} retain_height={}", pruned, retain_height));
    }

    // Reset verficiation cache
    cache.clear();

    // fire_events() // todo?

    return new_state_.value();
  }

  vote_extension extend_vote(vote& vote_) {
    auto req = request_extend_vote{vote_};
    auto res = proxyApp_->extend_vote_sync(req);
    return res.vote_extension_;
  }

  std::optional<std::string> verify_vote_extension(vote& vote_) {
    auto req = request_verify_vote_extension{vote_};
    auto res = proxyApp_->verify_vote_extension_sync(req); // todo - check error
    return {};
  }

  std::shared_ptr<abci_responses> exec_block_on_proxy_app(
    std::shared_ptr<app_connection> proxyAppConn, block& block_, db_store& db_store, int64_t initial_height) {
    uint valid_txs = 0, invalid_txs = 0, tx_index = 0;
    auto abci_responses_ = std::make_shared<abci_responses>();
    std::vector<response_deliver_tx> dtxs;
    dtxs.resize(block_.data.txs.size());
    abci_responses_->deliver_txs = dtxs;

    auto proxy_cb = [](/* req, res - todo */) {
      // todo - implement
    };
    // proxyAppConn.set_response_callback(proxy_cb); // todo

    auto commit_info = get_begin_block_validator_info(block_, store_, initial_height);

    // begin block
    abci_responses_->begin_block =
      proxyAppConn->begin_block_sync(requst_begin_block{block_.get_hash(), block_.header, commit_info});

    // run txs of block
    for (auto tx : block_.data.txs) {
      auto req = request_deliver_tx{};
      req.tx = bytes{}; // todo use tx
      proxyAppConn->deliver_tx_async(request_deliver_tx{req});
    }

    abci_responses_->end_block = proxyAppConn->end_block_sync(request_end_block{block_.header.height});

    ilog(fmt::format(
      "executed block: height={} num_valid_txs={} num_invalid_txs={}", block_.header.height, valid_txs, invalid_txs));
    return abci_responses_;
  }

  last_commit_info get_begin_block_validator_info(block& block_, db_store& store_, int64_t initial_height) {
    std::vector<vote_info> vote_infos;
    vote_infos.resize(block_.last_commit->size());
    if (block_.header.height > initial_height) {
      validator_set last_val_set;
      if (!store_.load_validators(block_.header.height - 1, last_val_set)) {
        throw std::runtime_error("panic");
      }

      // Check if commit_size matches validator_set size
      auto commit_size = block_.last_commit->size();
      auto val_set_len = last_val_set.validators.size();
      if (commit_size != val_set_len) {
        throw std::runtime_error("panic: commit_size doesn't match val_set length");
      }

      for (auto i = 0; i < last_val_set.validators.size(); i++) {
        auto commit_sig = block_.last_commit->signatures[i];
        vote_infos[i] = vote_info{last_val_set.validators[i], !commit_sig.absent()};
      }
    }
    return last_commit_info{block_.last_commit->round, vote_infos};
  }

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

  std::optional<state> update_state(state& state_, p2p::block_id block_id_, block_header& header_,
    std::shared_ptr<abci_responses> abci_responses_, std::vector<validator>& validator_updates) {
    // Copy val_set so that changes from end_block can be applied
    auto n_val_set = state_.next_validators;

    auto last_height_vals_changed = state_.last_height_validators_changed;
    if (!validator_updates.empty()) {
      n_val_set.update_with_change_set(validator_updates, true);
      last_height_vals_changed = header_.height + 1 + 1;
    }

    // Update validator proposer priority and set state variables
    n_val_set.increment_proposer_priority(1);

    // Update params with latest abci_responses
    auto next_params = state_.consensus_params_;
    auto last_height_params_changed = state_.last_height_validators_changed;
    if (abci_responses_->end_block.consensus_param_updates.has_value()) {
      // Note: must not mutate consensus_params
      next_params = abci_responses_->end_block.consensus_param_updates.value(); // todo - check if this is correct
      auto err = next_params.validate_consensus_params();
      if (err.has_value()) {
        elog(fmt::format("error updating consensus_params: {}", err.value()));
        return {};
      }

      state_.version = next_params.version.app_version;

      // Change results from this height
      last_height_vals_changed = header_.height + 1;
    }

    auto next_version = state_.version;

    state ret{};
    ret.version = next_version;
    ret.chain_id = state_.chain_id;
    ret.initial_height = state_.initial_height;
    ret.last_block_height = header_.height;
    ret.last_block_id = block_id_;
    ret.last_block_time = header_.time;
    ret.next_validators = n_val_set;
    ret.validators = state_.next_validators;
    ret.last_validators = state_.validators;
    ret.last_height_validators_changed = last_height_vals_changed;
    ret.consensus_params_ = next_params;
    ret.last_height_consensus_params_changed = last_height_params_changed;
    // ret.last_result_hash = store::abci_responses_result_hash(abci_responses_); // todo
    ret.app_hash.clear();
    return ret;
  }

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
