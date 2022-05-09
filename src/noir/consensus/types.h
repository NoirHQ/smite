// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/common/time.h>
#include <noir/consensus/protocol.h>
#include <noir/consensus/types/genesis.h>
#include <noir/consensus/types/params.h>
#include <noir/consensus/types/vote.h>

namespace noir::consensus {

struct round_vote_set {
  std::shared_ptr<vote_set> prevotes;
  std::shared_ptr<vote_set> precommits;
};

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
  tstamp time;
  int64_t weight;
};

} // namespace noir::consensus

NOIR_REFLECT(std::chrono::system_clock::duration, );
NOIR_REFLECT_DERIVED(noir::consensus::proposal, p2p::proposal_message);
