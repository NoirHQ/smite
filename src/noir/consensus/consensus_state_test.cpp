// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/consensus_state.h>
#include <stdlib.h>

using namespace noir::consensus;

config config_setup() {
  auto config_ = config::default_config();
  config_.base.chain_id = "test_chain";
  return config_;
}

std::unique_ptr<consensus_state> rand_cs(config& config_, int validator_number) {
  auto state_ = state::make_genesis_state();
  for (auto i = 0; i < validator_number; i++)
    state_.validators.validators.push_back(validator{noir::from_hex("AAAA" + to_string(i)), {}, std::rand(), 0});
  return consensus_state::new_state(config_.consensus, state_);
}

void start_test_round(std::unique_ptr<consensus_state>& cs, int64_t height, int32_t round) {
  cs->enter_new_round(height, round);
  //  cs.startRoutines(0) // not needed for noir
}

TEST_CASE("Basic", "[consensus_state]") {
  auto local_config = config_setup();
  auto cs1 = rand_cs(local_config, 4);

  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  start_test_round(cs1, height, round);

  auto rs = cs1->get_round_state();
  auto prop = rs->validators->get_proposer();

  //  cs1->schedule_timeout(std::chrono::milliseconds{3000}, 1, 0, NewHeight);
  //  cs1->schedule_timeout(std::chrono::milliseconds{4000}, 1, 1, Propose);
}
