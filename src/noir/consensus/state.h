// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/consensus/common.h>
#include <noir/consensus/tx.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/genesis.h>
#include <noir/consensus/types/validator.h>
#include <noir/consensus/version.h>

namespace noir::consensus {

struct weighted_time {
  tstamp time;
  int64_t weight;
};

/**
 * State is a short description of the latest committed block of the Tendermint consensus.
 * It keeps all information necessary to validate new blocks,
 * including the last validator set and the consensus params.
 */
struct state {
  ::noir::consensus::version version;

  std::string chain_id;
  int64_t initial_height;

  int64_t last_block_height{0}; // set to 0 at genesis
  p2p::block_id last_block_id;
  tstamp last_block_time;

  std::shared_ptr<validator_set> validators{};
  std::shared_ptr<validator_set> next_validators{};
  std::shared_ptr<validator_set> last_validators{};
  int64_t last_height_validators_changed;

  consensus_params consensus_params_;
  int64_t last_height_consensus_params_changed;

  bytes last_result_hash;

  bytes app_hash;

  static state make_genesis_state(genesis_doc& gen_doc) {
    if (!gen_doc.validate_and_complete())
      return state{};

    std::vector<validator> empty_set;
    std::shared_ptr<validator_set> val_set, next_val_set;
    if (gen_doc.validators.empty()) {
      val_set = validator_set::new_validator_set(empty_set);
      next_val_set = validator_set::new_validator_set(empty_set);
    } else {
      std::vector<validator> validators;
      for (const auto& val : gen_doc.validators) {
        validators.push_back(validator{val.address, val.pub_key, val.power, 0});
      }
      val_set = validator_set::new_validator_set(validators);
      next_val_set = validator_set::new_validator_set(validators)->copy_increment_proposer_priority(1);
    }

    state state_{};
    state_.version = version::init_state_version();
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

  std::tuple<std::shared_ptr<block>, std::shared_ptr<part_set>> make_block(int64_t height,
    std::vector<bytes>& txs,
    const std::shared_ptr<commit>& commit_,
    /* evidence, */ bytes proposal_address) {
    // Build base block
    auto block_ = block::make_block(height, txs, commit_);

    // Set time
    tstamp timestamp;
    if (height == initial_height) {
      timestamp = last_block_time; // genesis_time;
    } else {
      timestamp = get_median_time(commit_, last_validators);
    }

    // Fill rest of header
    block_->header.populate(version.cs, chain_id, timestamp, last_block_id, validators->get_hash(),
      next_validators->get_hash(), consensus_params_.hash_consensus_params(), app_hash, last_result_hash,
      proposal_address);

    return {block_, block_->make_part_set(block_part_size_bytes)};
  }

  tstamp get_median_time(const std::shared_ptr<commit>& commit_, const std::shared_ptr<validator_set>& validators) {
    std::vector<weighted_time> weighted_times;
    int64_t total_voting_power{};
    for (auto commit_sig : commit_->signatures) {
      if (commit_sig.absent())
        continue;
      auto validator = validators->get_by_address(commit_sig.validator_address);
      if (validator.has_value()) {
        total_voting_power += validator->voting_power;
        weighted_times.push_back(weighted_time{commit_sig.timestamp, validator->voting_power});
      }
    }
    return weighted_median(weighted_times, total_voting_power);
  }

  tstamp weighted_median(std::vector<weighted_time>& weight_times, int64_t total_voting_power) {
    auto median = total_voting_power / 2;
    sort(weight_times.begin(), weight_times.end(), [](weighted_time a, weighted_time b) { return a.time < b.time; });
    tstamp res = 0;
    for (auto t : weight_times) {
      if (median <= t.weight) {
        res = t.time;
        break;
      }
      median -= t.weight;
    }
    return res;
  }

  bool is_empty() const {
    return validators == nullptr;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::state, version, chain_id, initial_height, last_block_height, last_block_id,
  last_block_time, validators, next_validators, last_validators, last_height_validators_changed, consensus_params_,
  last_height_consensus_params_changed, last_result_hash, app_hash);
