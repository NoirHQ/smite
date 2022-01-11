// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block.h>
#include <noir/consensus/params.h>
#include <noir/consensus/tx.h>
#include <noir/consensus/validator.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

using namespace std;

/**
 * State is a short description of the latest committed block of the Tendermint consensus.
 * It keeps all information necessary to validate new blocks,
 * including the last validator set and the consensus params.
 */
class state {
public:
  state();

  block make_block(int64_t height, std::vector<tx> txs, commit commit, /* evidence, */ bytes proposal_address);

  tstamp get_median_time();

  state update_state(state new_state, block_id new_block_id, /* header, */ /* abci_response, */
    std::vector<validator> validator_updates);

public:
  string version;

  string chain_id;
  int64_t initial_height;

  int64_t last_block_height{0}; // set to 0 at genesis
  block_id last_block_id;
  tstamp last_block_time;

  validator_set validators; // persisted to the database separately every time they change, so we can query for
                            // historical validator sets.
  validator_set next_validators;
  validator_set last_validators; // used to validate block.LastCommit
  int64_t last_height_validators_changed;

  consensus_params consensus_params;
  int64_t last_height_consensus_params_changed;

  bytes last_result_hash;

  bytes app_hash;
};

state::state() {
  // read from config file, or restore from last saved state
  validator_set validatorSet, nextValidatorSet;
  //  if genDoc.Validators == nil || len(genDoc.Validators) == 0
  //  {
  //    validatorSet = types.NewValidatorSet(nil)
  //    nextValidatorSet = types.NewValidatorSet(nil)
  //  } else {

  // read from genesis.json
  //    validatorSet = types.NewValidatorSet(validators)
  //    nextValidatorSet = types.NewValidatorSet(validators).CopyIncrementProposerPriority(1)
  //  }

  version = "0.0.0";
  //  initial_height = genDoc.inital_height;
  last_block_height = 0;
  //  last_block_time = genDoc.GenesisTime;

  next_validators = nextValidatorSet;
  validators = validatorSet;
  //  last_validators = nil;
  //  last_height_validators_changed = genDoc.initial_height;

  //  consensus_params = genDoc.consensus_params;
  //  last_height_consensus_params_changed = genDoc.initial_height;

  //  app_hash = genDoc.app_hash;
}

block state::make_block(int64_t height, std::vector<tx> txs, commit commit, /* evidence, */ bytes proposal_address) {
  // Set time
  tstamp timestamp;
  //    if (height == initial_height) {
  //      timestamp = genesis_time;
  //    } else {
  //      timestamp = get_median_time();
  //    }
}

tstamp state::get_median_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

state state::update_state(state new_state, block_id new_block_id, /* header, */ /* abci_response, */
  std::vector<validator> validator_updates) {

  //  next_validators.update_with_change_set();

#if 0
  // Copy the valset so we can apply changes from EndBlock
  // and update s.LastValidators and s.Validators.
  nValSet := state.NextValidators.Copy()

  // Update the validator set with the latest abciResponses.
  lastHeightValsChanged := state.LastHeightValidatorsChanged
  if len(validatorUpdates) > 0 {
    err := nValSet.UpdateWithChangeSet(validatorUpdates)
    if err != nil {
      return state, fmt.Errorf("error changing validator set: %v", err)
    }
    // Change results from this height but only applies to the next next height.
    lastHeightValsChanged = header.Height + 1 + 1
  }

  // Update validator proposer priority and set state variables.
  nValSet.IncrementProposerPriority(1)

  // Update the params with the latest abciResponses.
  nextParams := state.ConsensusParams
  lastHeightParamsChanged := state.LastHeightConsensusParamsChanged
  if abciResponses.EndBlock.ConsensusParamUpdates != nil {
    // NOTE: must not mutate s.ConsensusParams
    nextParams = state.ConsensusParams.UpdateConsensusParams(abciResponses.EndBlock.ConsensusParamUpdates)
    err := nextParams.ValidateConsensusParams()
    if err != nil {
      return state, fmt.Errorf("error updating consensus params: %v", err)
    }

    state.Version.Consensus.App = nextParams.Version.AppVersion

    // Change results from this height but only applies to the next height.
    lastHeightParamsChanged = header.Height + 1
  }

  nextVersion := state.Version
#endif
}

} // namespace noir::consensus
