// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/validator.h>
#include <noir/consensus/block.h>
#include <noir/consensus/consensus_types.h>
#include <noir/p2p/types.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

enum round_step_type {
  NewHeight = 1, // Wait til CommitTime + timeoutCommit
  NewRound = 2, // Setup new round and go to RoundStepPropose
  Propose = 3, // Did propose, gossip proposal
  Prevote = 4, // Did prevote, gossip prevotes
  PrevoteWait = 5, // Did receive any +2/3 prevotes, start timeout
  Precommit = 6, // Did precommit, gossip precommits
  PrecommitWait = 7, // Did receive any +2/3 precommits, start timeout
  Commit = 8  // Entered commit state machine
};

/*
 * Defines the internal consensus state.
 * NOTE: not thread safe
 */
struct round_state {
  int64_t height;
  int32_t round;
  round_step_type step;
  tstamp start_time;

  // Subjective time when +2/3 precommits for Block at Round were found
  tstamp commit_time;
  validator_set validators;
  proposal_message proposal;
  block proposal_block;
  part_set proposal_block_parts;
  int32_t locked_round;
  block locked_block;
  part_set locked_block_parts;

  // Last known round with POL for non-nil valid block.
  int32_t valid_round;
  block valid_block; // Last known block of POL mentioned above.

  part_set valid_block_parts;
  height_vote_set votes;
  int32_t commit_round;
  vote_set last_commit;
  validator_set las_validators;
  bool triggered_timeout_precommit;
};

} // namespace noir::consensus
