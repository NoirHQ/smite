// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/height_vote_set.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

/**
 * Defines the internal consensus state.
 * NOTE: not thread safe
 */
struct round_state {
  int64_t height;
  int32_t round;
  p2p::round_step_type step;
  p2p::tstamp start_time;

  // Subjective time when +2/3 precommits for Block at Round were found
  p2p::tstamp commit_time;
  std::shared_ptr<validator_set> validators{};
  std::shared_ptr<p2p::proposal_message> proposal{};
  std::shared_ptr<block> proposal_block{};
  std::shared_ptr<part_set> proposal_block_parts{};
  int32_t locked_round;
  std::shared_ptr<block> locked_block{};
  std::shared_ptr<part_set> locked_block_parts{};

  // Last known round with POL for non-nil valid block.
  int32_t valid_round;
  std::shared_ptr<block> valid_block{}; // Last known block of POL mentioned above.

  std::shared_ptr<part_set> valid_block_parts{};
  std::shared_ptr<height_vote_set> votes{};
  int32_t commit_round;
  std::shared_ptr<vote_set> last_commit{};
  std::shared_ptr<validator_set> last_validators{};
  bool triggered_timeout_precommit;
};

} // namespace noir::consensus
