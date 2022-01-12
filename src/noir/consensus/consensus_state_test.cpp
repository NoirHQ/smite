// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/consensus_state.h>

using namespace noir::consensus;

TEST_CASE("Basic", "[consensus_state]") {
  auto local_config = config::default_config();
  state prev_state;
  std::unique_ptr<consensus_state> cs_state = consensus_state::new_state(local_config.consensus, prev_state);
  cs_state->schedule_timeout(std::chrono::milliseconds{3000}, 1, 0, NewHeight);
  cs_state->schedule_timeout(std::chrono::milliseconds{4000}, 1, 1, Propose);
}
