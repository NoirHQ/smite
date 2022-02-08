// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/state.h>

namespace noir::consensus {

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

  //  consensus_params_ = genDoc.consensus_params;
  //  last_height_consensus_params_changed = genDoc.initial_height;

  //  app_hash = genDoc.app_hash;
}

std::tuple<block, part_set> state::make_block(
  int64_t height, std::vector<bytes> txs, commit commit, /* evidence, */ bytes proposal_address) {
  // Set time
  p2p::tstamp timestamp;
  //    if (height == initial_height) {
  //      timestamp = genesis_time;
  //    } else {
  //      timestamp = get_median_time();
  //    }
  return {block{}, part_set{}};
}

p2p::tstamp state::get_median_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

state state::update_state(state new_state, p2p::block_id new_block_id, /* header, */ /* abci_response, */
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
  return state{};
}

bool state::is_empty() {
  return validators.validators.empty();
}

state state::make_genesis_state(genesis_doc& gen_doc) {
  // todo - read from genDoc
  state state_{};

  std::vector<validator> validators;
  validator_set val_set;
  for (const auto& val : gen_doc.validators) {
    validators.push_back(validator{val.address, val.pub_key_, val.power, 0});
  }
  val_set = validator_set::new_validator_set(validators);
  validator_set next_val_set = val_set.copy_increment_proposer_priority(1);
  state_.validators = val_set;
  state_.next_validators = next_val_set;

  state_.chain_id = gen_doc.chain_id;
  state_.initial_height = gen_doc.initial_height;
  return state_;
}

} // namespace noir::consensus
