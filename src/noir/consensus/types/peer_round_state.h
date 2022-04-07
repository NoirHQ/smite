// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/bit_array.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

/// \brief contains known state of a peer
struct peer_round_state {
  int64_t height{};
  int32_t round{};
  p2p::round_step_type step{};

  p2p::tstamp start_time{};

  bool proposal{};
  p2p::part_set_header proposal_block_part_set_header;
  std::shared_ptr<bit_array> proposal_block_parts;
  int32_t proposal_pol_round{};

  std::shared_ptr<bit_array> proposal_pol;
  std::shared_ptr<bit_array> prevotes;
  std::shared_ptr<bit_array> precommits;
  int32_t last_commit_round{};
  std::shared_ptr<bit_array> last_commit;

  int32_t catchup_commit_round{};

  std::shared_ptr<bit_array> catchup_commit;

  peer_round_state() = default;
  peer_round_state(const peer_round_state&) = default;
  peer_round_state& operator=(const peer_round_state& v) {
    proposal_block_part_set_header = v.proposal_block_part_set_header; // TODO: check if this is correct
    height = v.height;
    round = v.round;
    step = v.step;
    start_time = v.start_time;
    proposal = v.proposal;
    proposal_pol_round = v.proposal_pol_round;
    last_commit_round = v.last_commit_round;
    catchup_commit_round = v.catchup_commit_round;
    proposal_block_parts = bit_array::copy(v.proposal_block_parts);
    proposal_pol = bit_array::copy(v.proposal_pol);
    prevotes = bit_array::copy(v.prevotes);
    precommits = bit_array::copy(v.precommits);
    last_commit = bit_array::copy(v.last_commit);
    catchup_commit = bit_array::copy(v.catchup_commit);
    return *this;
  }
};

} // namespace noir::consensus
