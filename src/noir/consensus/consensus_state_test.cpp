// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>

using namespace noir::consensus;

TEST_CASE("Proposer Selection 0", "[consensus_state]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 4);

  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  start_test_round(cs1, height, round);

  force_tick(cs1); // ensureNewRound

  auto rs = cs1->get_round_state();
  auto prop = rs->validators->get_proposer();
  auto pv = cs1->local_priv_validator->get_pub_key();

  auto addr = pv.address();

  CHECK(prop->address == addr);

  // wait for complete proposal // todo - how?
  force_tick(cs1); // ensureNewProposal

  rs = cs1->get_round_state();

  force_tick(cs1); // ensureNewProposal
  rs = cs1->get_round_state();

  //  cs1->schedule_timeout(std::chrono::milliseconds{3000}, 1, 0, NewHeight);
  //  cs1->schedule_timeout(std::chrono::milliseconds{4000}, 1, 1, Propose);
}

TEST_CASE("No Priv Validator", "[consensus_state]") {
  auto local_config = config_setup();
  auto [cs1, vss] = rand_cs(local_config, 1);
  cs1->local_priv_validator = {};

  auto height = cs1->rs.height;
  auto round = cs1->rs.round;

  start_test_round(cs1, height, round);

  force_tick(cs1); // ensureNewRound

  CHECK(!cs1->get_round_state()->proposal.has_value());
}
