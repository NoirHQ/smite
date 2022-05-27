// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <noir/consensus/app_connection.h>
#include <noir/consensus/common.h>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/types/event_bus.h>
#include <noir/consensus/types/events.h>
#include <noir/tx_pool/tx_pool.h>

#include <utility>

namespace noir::consensus {

/**
 * Provides functions for executing a block and updates state and mempool
 */
struct block_executor {

  std::shared_ptr<db_store> store_{};

  std::shared_ptr<block_store> block_store_{};

  std::shared_ptr<app_connection> proxyApp_{};

  // evidence tool // todo
  // todo - we may not need to handle events for tendermint but only for app?
  std::shared_ptr<events::event_bus> event_bus_{};

  std::map<std::string, bool> cache; // storing verification result for a single height

  block_executor(std::shared_ptr<db_store> new_store,
    std::shared_ptr<app_connection> new_proxyApp,
    std::shared_ptr<block_store> new_block_store,
    std::shared_ptr<events::event_bus> new_event_bus)
    : store_(std::move(new_store)),
      block_store_(std::move(new_block_store)),
      proxyApp_(std::move(new_proxyApp)),
      event_bus_(new_event_bus) {}

  static std::shared_ptr<block_executor> new_block_executor(const std::shared_ptr<db_store>& new_store,
    const std::shared_ptr<app_connection>& new_proxyApp,
    const std::shared_ptr<block_store>& new_block_store,
    const std::shared_ptr<events::event_bus>& new_event_bus) {
    auto res = std::make_shared<block_executor>(new_store, new_proxyApp, new_block_store, new_event_bus);
    return res;
  }

  std::tuple<std::shared_ptr<block>, std::shared_ptr<part_set>> create_proposal_block(
    int64_t height, state& state_, commit& commit_, bytes& proposer_addr, std::vector<std::optional<vote>>& votes) {
    auto max_bytes = state_.consensus_params_.block.max_bytes;
    auto max_gas = state_.consensus_params_.block.max_gas;

    // evidence // todo

    // Fetch a limited amount of valid txs
    auto max_data_bytes =
      max_bytes - max_overhead_for_block - max_header_bytes - max_commit_bytes(state_.validators.size());
    if (max_data_bytes < 0)
      throw std::runtime_error(fmt::format(
        "negative max_data_bytes: max_bytes={} is too small to accommodate header and last_commit", max_bytes));

    std::vector<bytes> txs;
    auto prepared_proposal = proxyApp_->prepare_proposal_sync(request_prepare_proposal{txs, max_data_bytes, votes});
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

  bool validate_block(state& state_, std::shared_ptr<block> block_) {
    auto hash = block_->get_hash();
    if (cache.find(to_hex(hash)) != cache.end())
      return true;

    /// Validate block
    if (auto err = block_->validate_basic(); err.has_value()) {
      elog(fmt::format("invalid header: {}", err.value()));
      return false;
    }

    /// todo - activate all of the following validations
    // Check basic info
    // if (block_->version != state_.version) {
    //  elog("wrong block_header_version");
    //  return false;
    // }
    if (state_.last_block_height == 0 && block_->header.height != state_.initial_height) {
      elog("wrong block_header_height");
      return false;
    }
    if (state_.last_block_height > 0 && block_->header.height != state_.last_block_height + 1) {
      elog("wrong block_header_height");
      return false;
    }

    // if (block_->header.last_block_id != state_.last_block_id) {
    //   elog("wrong block_header_last_block_id");
    //   return false;
    // }

    // Check app info
    if (block_->header.app_hash != state_.app_hash) {
      elog("wrong block_header_app_hash");
      return false;
    }
    auto hash_cp = state_.consensus_params_.hash_consensus_params();
    if (block_->header.consensus_hash != hash_cp) {
      elog("wrong block_header_consensus_hash");
      return false;
    }
    if (block_->header.last_results_hash != state_.last_result_hash) {
      elog("wrong block_header_last_results_hash");
      return false;
    }
    // todo - more

    // Check block time
    if (block_->header.height > state_.initial_height) {
      // if (block_->header.time > state_.last_block_time) {
      //   elog("block time is not greater than last block time");
      //   return false;
      // }
      // auto median_time = get_median_time(block_->last_commit, state_.last_validators);
      // if (block_->header.time != median_time) {
      //   elog("invalid block time");
      //   return false;
      // }
    } else if (block_->header.height == state_.initial_height) {
      auto genesis_time = state_.last_block_time;
      // if (block_->header.time != genesis_time) {
      //   elog("block time is not equal to genesis time");
      //   return false;
      // }
    } else {
      elog("block height is lower than initial height");
      return false;
    }
    /// End of validate block

    // check evidence // todo

    cache[to_hex(hash)] = true;
    return true;
  }

  std::optional<state> apply_block(state& state_, p2p::block_id block_id_, std::shared_ptr<block> block_) {
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

    if (!store_->save_abci_responses(block_->header.height, *abci_responses_)) {
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

    auto new_state_ = update_state(state_, block_id_, block_->header, abci_responses_, validator_updates.value());
    if (!new_state_.has_value()) {
      elog("apply block failed: commit failed for application");
      return {};
    }

    /// directly implement commit()
    // mempool: lock & auto unlock - todo - maybe not needed for noir?
    // mempool: flush_app_conn() - todo - maybe not needed for noir?

    // Commit block and get hash
    auto commit_res = proxyApp_->commit_sync();

    ilog(fmt::format(
      "committed state: height={}, num_txs... app_hash={}", block_->header.height, to_hex(commit_res.data)));

    // Update mempool
    std::vector<consensus::tx> block_txs;

    bytes app_hash = commit_res.data;
    int64_t retain_height = commit_res.retain_height;
    /// commit() ends

    // Update app_hash and save the state
    new_state_->app_hash = app_hash;
    if (!store_->save(new_state_.value())) {
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

    // fire_events()
    fire_events(*block_, block_id_, *abci_responses_, *validator_updates);

    return new_state_.value();
  }

  std::shared_ptr<abci_responses> exec_block_on_proxy_app(std::shared_ptr<app_connection> proxyAppConn,
    std::shared_ptr<block> block_,
    std::shared_ptr<db_store> db_store,
    int64_t initial_height) {
    uint valid_txs = 0, invalid_txs = 0;
    auto abci_responses_ = std::make_shared<abci_responses>();
    std::vector<response_deliver_tx> dtxs;
    dtxs.resize(block_->data.txs.size());
    abci_responses_->deliver_txs = dtxs;

    // todo - use get_begin_block_validator_info() below, right now it throws a panic
    // auto commit_info = get_begin_block_validator_info(block_, store_, initial_height);
    last_commit_info commit_info; // todo - remove later

    // begin block
    abci_responses_->begin_block =
      proxyAppConn->begin_block_sync(request_begin_block{block_->get_hash(), block_->header, commit_info});

    // run txs of block
    for (const auto& tx : block_->data.txs) {
      auto& deliver_res_req = proxyAppConn->deliver_tx_async(request_deliver_tx{tx});
      /* todo - verify if implementation is correct;
       *        basically removed the original callback func and directly applied it here */
      auto deliver_res = deliver_res_req.res;
      if (deliver_res.code == code_type_ok) {
        valid_txs++;
      } else {
        dlog(fmt::format("invalid tx: code=", deliver_res.code));
        invalid_txs++;
      }
      abci_responses_->deliver_txs.push_back(deliver_res);
    }

    abci_responses_->end_block = proxyAppConn->end_block_sync(request_end_block{block_->header.height});

    ilog(fmt::format(
      "executed block: height={} num_valid_txs={} num_invalid_txs={}", block_->header.height, valid_txs, invalid_txs));
    return abci_responses_;
  }

  last_commit_info get_begin_block_validator_info(
    block& block_, std::shared_ptr<db_store> store_, int64_t initial_height) {
    std::vector<vote_info> vote_infos;
    if (!block_.last_commit) {
      elog("get_begin_block_validator_info failed: no last_commit");
      return {};
    }
    vote_infos.resize(block_.last_commit->size());
    if (block_.header.height > initial_height) {
      validator_set last_val_set;
      if (!store_->load_validators(block_.header.height - 1, last_val_set)) {
        throw std::runtime_error(
          fmt::format("panic: unable to load validator for height={}", block_.header.height - 1));
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

  std::optional<state> update_state(state& state_,
    p2p::block_id block_id_,
    block_header& header_,
    std::shared_ptr<abci_responses> abci_responses_,
    std::vector<validator>& validator_updates) {
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

      state_.version.cs.app = next_params.version.app_version;

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

  /// Fire NewBlock, NewBlockHeader.
  /// Fire TxEvent for every tx.
  /// \note if Tendermint crashes before commit, some or all of these events may be published again.
  void fire_events(const block& block_,
    const p2p::block_id& block_id_,
    const abci_responses& abci_rsp,
    const std::vector<validator> val_updates) {
    event_bus_->publish_event_new_block(events::event_data_new_block{
      .block = block_,
      .block_id = block_id_,
      .result_begin_block = abci_rsp.begin_block,
      .result_end_block = abci_rsp.end_block,
    });

    event_bus_->publish_event_new_block_header(events::event_data_new_block_header{
      .header = block_.header,
      .num_txs = static_cast<int64_t>(block_.data.txs.size()),
      .result_begin_block = abci_rsp.begin_block,
      .result_end_block = abci_rsp.end_block,
    });

    // TODO: evidence
    // if (block_.evidence.evidence.size()) {
    //   for (auto ev : block_.evidence.evidence) {
    //     event_bus_->publish_event_new_evidence(events::event_data_new_evidence{
    //       .evidence = ev,
    //       .height = block_.header.height,
    //     });
    //   }
    // }

    for (uint32_t i = 0; auto tx : block_.data.txs) {
      event_bus_->publish_event_tx(events::event_data_tx{
        .tx_result =
          tx_result{
            .height = block_.header.height,
            .index = i,
            .tx = tx,
            .result = abci_rsp.deliver_txs[i],
          },
      });
      ++i;
    }

    if (val_updates.size() > 0) {
      event_bus_->publish_event_validator_set_updates(events::event_data_validator_set_updates{
        .validator_updates = val_updates,
      });
    }
  }

  uint64_t prune_blocks(int64_t retain_height) {
    auto base = block_store_->base();
    if (retain_height <= base)
      return 0;
    uint64_t pruned;
    if (!block_store_->prune_blocks(retain_height, pruned)) {
      elog("filed to prune block store");
      return 0;
    }
    if (!store_->prune_states(retain_height)) {
      elog("filed to prune state store");
      return 0;
    }
    return pruned;
  }
};

} // namespace noir::consensus
