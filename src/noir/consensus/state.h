// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/consensus/block.h>
#include <noir/consensus/tx.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

/**
 * State is a short description of the latest committed block of the Tendermint consensus.
 * It keeps all information necessary to validate new blocks,
 * including the last validator set and the consensus params.
 */
struct state {
  std::string version{"0.0.0"};

  std::string chain_id;
  int64_t initial_height;

  int64_t last_block_height{0}; // set to 0 at genesis
  p2p::block_id last_block_id;
  p2p::tstamp last_block_time;

  validator_set validators; // persisted to the database separately every time they change, so we can query for
                            // historical validator sets.
  validator_set next_validators;
  validator_set last_validators; // used to validate block.LastCommit
  int64_t last_height_validators_changed;

  consensus_params consensus_params_;
  int64_t last_height_consensus_params_changed;

  bytes last_result_hash;

  bytes app_hash;

  std::tuple<std::shared_ptr<block>, std::shared_ptr<part_set>> make_block(
    int64_t height, std::vector<bytes> txs, commit commit_, /* evidence, */ bytes proposal_address) {
    // Build base block
    auto block_ = block::make_block(height, txs, commit_);

    // Set time
    p2p::tstamp timestamp;
    if (height == initial_height) {
      timestamp = last_block_time; // genesis_time;
    } else {
      timestamp = get_median_time(commit_, last_validators);
    }

    // Fill rest of header
    block_->header.populate(version, chain_id, timestamp, last_block_id, validators.get_hash(),
      next_validators.get_hash(), consensus_params_.hash_consensus_params(), app_hash, last_result_hash,
      proposal_address);

    return {block_, block_->make_part_set(block_part_size_bytes)};
  }

  p2p::tstamp get_median_time(commit& commit_, validator_set& validators) {
    std::vector<weighted_time> weighted_times;
    int64_t total_voting_power{};
    for (auto commit_sig : commit_.signatures) {
      if (commit_sig.absent())
        continue;
      auto validator = validators.get_by_address(commit_sig.validator_address);
      if (validator.has_value()) {
        total_voting_power += validator->voting_power;
        weighted_times.push_back(weighted_time{commit_sig.timestamp, validator->voting_power});
      }
    }
    return weighted_median(weighted_times, total_voting_power);
  }

  bool is_empty() {
    return validators.validators.empty();
  }

  static state make_genesis_state(genesis_doc& gen_doc) {
    if (!gen_doc.validate_and_complete())
      return state{};

    std::vector<validator> empty_set;
    validator_set val_set, next_val_set;
    if (gen_doc.validators.empty()) {
      val_set = validator_set::new_validator_set(empty_set);
      next_val_set = validator_set::new_validator_set(empty_set);
    } else {
      std::vector<validator> validators;
      for (const auto& val : gen_doc.validators) {
        validators.push_back(validator{val.address, val.pub_key_, val.power, 0});
      }
      val_set = validator_set::new_validator_set(validators);
      next_val_set = validator_set::new_validator_set(validators).copy_increment_proposer_priority(1);
    }

    state state_{};
    state_.chain_id = gen_doc.chain_id;
    state_.initial_height = gen_doc.initial_height;

    state_.last_block_height = 0;
    state_.last_block_id = p2p::block_id{};
    state_.last_block_time = gen_doc.genesis_time;

    state_.validators = val_set;
    state_.next_validators = next_val_set;
    state_.last_validators = validator_set::new_validator_set(empty_set);
    state_.last_height_validators_changed = gen_doc.initial_height;

    state_.consensus_params_ = gen_doc.cs_params.value();
    state_.last_height_consensus_params_changed = gen_doc.initial_height;
    return state_;
  }
};

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(noir::consensus::state, version, chain_id, initial_height, last_block_height, last_block_id,
  last_block_time, validators, next_validators, last_validators, last_height_validators_changed, consensus_params_,
  last_height_consensus_params_changed, last_result_hash, app_hash)
