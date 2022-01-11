// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/validator.h>
#include <noir/consensus/block.h>
#include <noir/consensus/vote_set.h>
#include <noir/p2p/types.h>
#include <noir/p2p/protocol.h>

#include <appbase/application.hpp>
#include <appbase/channel.hpp>

namespace noir::consensus {

using namespace noir::p2p;

struct part {
  uint32_t index;
  bytes bytes;
  //proof proof;
};

struct part_set {
  uint32_t total;
  bytes hash;

//  std::mutex mtx;
  std::vector<part> parts;
  //bit_array parts_bit_array;
  uint32_t count;
  int64_t byte_size;
};

struct round_vote_set {
  vote_set prevotes;
  vote_set precommits;
};

/**
 * Keeps track of all VoteSets from round 0 to round 'round'.
 * Also keeps track of up to one RoundVoteSet greater than
 * 'round' from each peer, to facilitate catchup syncing of commits.
 *
 * A commit is +2/3 precommits for a block at a round,
 * but which round is not known in advance, so when a peer
 * provides a precommit for a round greater than mtx.round,
 * we create a new entry in roundVoteSets but also remember the
 * peer to prevent abuse.
 * We let each peer provide us with up to 2 unexpected "catchup" rounds.
 * One for their LastCommit round, and another for the official commit round.
 */
struct height_vote_set {
  std::string chain_id;
  int64_t height;
  validator_set val_set;

//  std::mutex mtx;
  int32_t round;
  std::map<int32_t, round_vote_set> round_vote_sets;
  std::map<node_id, int32_t> peer_catchup_rounds;
};

enum round_step_type {
  NewHeight = 1, // Wait til CommitTime + timeoutCommit
  NewRound = 2, // Setup new round and go to RoundStepPropose
  Propose = 3, // Did propose, gossip proposal
  Prevote = 4, // Did prevote, gossip prevotes
  PrevoteWait = 5, // Did receive any +2/3 prevotes, start timeout
  Precommit = 6, // Did precommit, gossip precommits
  PrecommitWait = 7, // Did receive any +2/3 precommits, start timeout
  Commit = 8 // Entered commit state machine
};

/**
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

struct timeout_info {
  std::chrono::system_clock::duration duration_;
  int64_t height;
  int32_t round;
  round_step_type step;
};

using timeout_info_ptr = std::shared_ptr<timeout_info>;

namespace channels {
using timeout_ticker = appbase::channel_decl<struct timeout_ticker_tag, timeout_info_ptr>;
}

} // namespace noir::consensus

FC_REFLECT(noir::consensus::timeout_info, (duration_)(height)(round)(step))
FC_REFLECT(std::chrono::system_clock::duration, )
FC_REFLECT(noir::consensus::round_step_type, )
