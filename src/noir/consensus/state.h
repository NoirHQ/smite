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
class state {
public:
  state();

  block make_block(int64_t height, std::vector<tx> txs, commit commit, /* evidence, */ bytes proposal_address);

  p2p::tstamp get_median_time();

  state update_state(state new_state, p2p::block_id new_block_id, /* header, */ /* abci_response, */
    std::vector<validator> validator_updates);

  bool is_empty();

  static state make_genesis_state(genesis_doc& gen_doc);

public:
  std::string version;

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
};

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(noir::consensus::state, version, chain_id, initial_height, last_block_height, last_block_id,
  last_block_time, next_validators, validators, last_validators, last_height_validators_changed, consensus_params_,
  last_height_consensus_params_changed, last_result_hash, app_hash);
