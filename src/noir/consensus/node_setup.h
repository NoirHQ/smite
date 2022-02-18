// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/config.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/state.h>

namespace noir::consensus {

void log_node_startup_info(state& state_, pub_key& pub_key_, node_mode mode) {
  ilog(fmt::format("Version info: version={}, mode={}", state_.version, mode_str(mode)));
  switch (mode) {
  case Full:
    ilog("################################");
    ilog("### This node is a full_node ###");
    ilog("################################");
    break;
  case Validator:
    ilog("#####################################");
    ilog("### This node is a validator_node ###");
    ilog("#####################################");
    {
      auto addr = pub_key_.address();
      if (state_.validators.has_address(addr))
        ilog("   > node is in the active validator set");
      else
        ilog("   > node is NOT in the active validator set");
    }
    break;
  case Seed:
    ilog("################################");
    ilog("### This node is a seed_node ###");
    ilog("################################");
    break;
  case Unknown:
    ilog("#################################");
    ilog("### This node is unknown_mode ###");
    ilog("#################################");
    break;
  }
}

std::tuple<std::shared_ptr<consensus_reactor>, std::shared_ptr<consensus_state>> create_consensus_reactor(
  const std::shared_ptr<config>& config_, const std::shared_ptr<state>& state_,
  const std::shared_ptr<block_executor>& block_exec_, const std::shared_ptr<block_store>& block_store_,
  const priv_validator& priv_validator_, bool wait_sync) {
  auto cs_state = consensus_state::new_state(config_->consensus, *state_, block_exec_, block_store_);

  if (config_->base.mode == Validator)
    cs_state->set_priv_validator(priv_validator_);

  auto cs_reactor = consensus_reactor::new_consensus_reactor(cs_state, wait_sync);

  return {cs_reactor, cs_state};
}

} // namespace noir::consensus
