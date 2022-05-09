// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/consensus/protocol.h>
#include <noir/consensus/types/genesis.h>
#include <noir/consensus/types/params.h>
#include <noir/consensus/types/vote.h>

namespace noir::consensus {

struct round_vote_set {
  std::shared_ptr<vote_set> prevotes;
  std::shared_ptr<vote_set> precommits;
};

inline p2p::tstamp get_time() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

/**
 * Proposal defines a block proposal for the consensus.
 * It refers to the block by BlockID field.
 * It must be signed by the correct proposer for the given Height/Round
 * to be considered valid. It may depend on votes from a previous round,
 * a so-called Proof-of-Lock (POL) round, as noted in the POLRound.
 * If POLRound >= 0, then BlockID corresponds to the block that is locked in POLRound.
 */
struct proposal : p2p::proposal_message {

  static proposal new_proposal(int64_t height_, int32_t round_, int32_t pol_round_, p2p::block_id b_id_) {
    return proposal{p2p::Proposal, height_, round_, pol_round_, b_id_, get_time()};
  }
};

struct weighted_time {
  p2p::tstamp time;
  int64_t weight;
};

inline p2p::tstamp weighted_median(std::vector<weighted_time>& weight_times, int64_t total_voting_power) {
  auto median = total_voting_power / 2;
  sort(weight_times.begin(), weight_times.end(), [](weighted_time a, weighted_time b) { return a.time < b.time; });
  p2p::tstamp res = 0;
  for (auto t : weight_times) {
    if (median <= t.weight) {
      res = t.time;
      break;
    }
    median -= t.weight;
  }
  return res;
}

} // namespace noir::consensus

NOIR_REFLECT(std::chrono::system_clock::duration, );
NOIR_REFLECT_DERIVED(noir::consensus::proposal, p2p::proposal_message);
