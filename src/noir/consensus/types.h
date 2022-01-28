// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/params.h>
#include <noir/consensus/validator.h>
#include <noir/consensus/vote.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

#include <appbase/application.hpp>
#include <appbase/channel.hpp>
#include <fmt/core.h>

namespace noir::consensus {

struct genesis_validator {
  p2p::bytes address;
  pub_key pub_key_;
  int64_t power;
  std::string name;
};

struct genesis_doc {
  p2p::tstamp genesis_time;
  std::string chain_id;
  int64_t initial_height;
  consensus_params cs_params;
  std::vector<genesis_validator> validators;
  p2p::bytes app_hash;
  p2p::bytes app_state;
};

struct part {
  uint32_t index;
  p2p::bytes bytes_;
  p2p::bytes proof;
};

struct part_set {
  uint32_t total;
  p2p::bytes hash;

  //  std::mutex mtx;
  std::vector<part> parts;
  // bit_array parts_bit_array;
  uint32_t count;
  int64_t byte_size;

  static part_set new_part_set_from_header(p2p::part_set_header header) {
    std::vector<part> parts_;
    parts_.resize(header.total);
    return part_set{header.total, header.hash, parts_, 0, 0};
  }

  bool is_complete() {
    return count == total;
  }

  p2p::part_set_header header() {
    return p2p::part_set_header{total, hash};
  }

  bool has_header(p2p::part_set_header header_) {
    return header() == header_;
  }

  bool add_part(part part_) {
    // todo - lock mtx

    if (part_.index >= total) {
      dlog("error part set unexpected index");
      return false;
    }

    // If part already exists, return false.
    try {
      auto p = parts.at(part_.index);
      if (!p.bytes_.empty() || !p.proof.empty()) {
        return false;
      }
    } catch (std::out_of_range&) {
    }

    // Check hash proof // todo

    // Add part
    parts[part_.index] = part_;
    // parts_bit_array =
    count++;
    byte_size += part_.bytes_.size();
    return true;
  }

  part get_part(int index) {
    // todo - lock mtx
    return parts[index]; // todo - check range?
  }
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

  std::mutex mtx;
  int32_t round;
  std::map<int32_t, round_vote_set> round_vote_sets;
  std::map<p2p::node_id, std::vector<int32_t>> peer_catchup_rounds;

  static std::shared_ptr<height_vote_set> new_height_vote_set(
    std::string chain_id_, int64_t height_, const validator_set& val_set_) {
    auto hvs = std::make_shared<height_vote_set>();
    hvs->chain_id = chain_id_;
    hvs->reset(height_, val_set_);
    return hvs;
  }

  void reset(int64_t height_, const validator_set& val_set_) {
    std::lock_guard<std::mutex> g(mtx);
    height = height_;
    val_set = val_set_;
    add_round(0);
    round = 0;
  }

  void add_round(int32_t round_) {
    if (round_vote_sets.contains(round_))
      throw std::runtime_error("add_round() for an existing round");

    auto prevotes = vote_set::new_vote_set(chain_id, height, round_, p2p::Prevote, val_set);
    auto precommits = vote_set::new_vote_set(chain_id, height, round_, p2p::Precommit, val_set);
    round_vote_sets[round_] = round_vote_set{prevotes, precommits};
  }

  void set_round(int32_t round_) {
    std::lock_guard<std::mutex> g(mtx);
    auto new_round_ = round - 1; // todo - safe subtract
    if (round != 0 && round_ < new_round_)
      throw std::runtime_error("set_round() must increment round");
    for (auto r = new_round_; r <= round_; r++) {
      if (round_vote_sets.contains(r))
        continue; // Already exists because peer_catchup_rounds
      add_round(r);
    }
    round = round_;
  }

  std::optional<vote_set> get_vote_set(int32_t round_, p2p::signed_msg_type vote_type) {
    auto it = round_vote_sets.find(round_);
    if (it == round_vote_sets.end())
      return {};
    switch (vote_type) {
    case p2p::Prevote:
      return it->second.prevotes;
    case p2p::Precommit:
      return it->second.precommits;
    default:
      throw std::runtime_error(fmt::format("get_vote_set() unexpected vote type {}", vote_type));
    }
  }

  /**
   * return last round number or -1 if not exists
   */
  int32_t pol_info() {
    std::lock_guard<std::mutex> g(mtx);
    for (auto r = round; r >= 0; r--) {
      auto rvs = get_vote_set(r, p2p::Prevote);
      if (rvs->two_thirds_majority().has_value())
        return r;
    }
    return -1;
  }

  bool add_vote(vote vote_, p2p::node_id peer_id) {
    std::lock_guard<std::mutex> g(mtx);
    if (!p2p::is_vote_type_valid(vote_.type))
      return false;
    auto vote_set_ = get_vote_set(vote_.round, vote_.type);
    if (!vote_set_.has_value()) {
      auto it = peer_catchup_rounds.find(peer_id);
      if (it == peer_catchup_rounds.end() || it->second.size() >= 2) {
        // punish peer // todo - how?
        elog("peer has sent a vote that does not match our round for more than one round");
        return false;
      }
      add_round(vote_.round);
      vote_set_ = get_vote_set(vote_.round, vote_.type);
      it->second.push_back(vote_.round);
    }
    return vote_set_->add_vote(vote_);
  }

  std::optional<vote_set> prevotes(int32_t round_) {
    std::lock_guard<std::mutex> g(mtx);
    return get_vote_set(round_, p2p::Prevote);
  }

  std::optional<vote_set> precommits(int32_t round_) {
    std::lock_guard<std::mutex> g(mtx);
    return get_vote_set(round_, p2p::Precommit);
  }
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
  p2p::tstamp start_time;

  // Subjective time when +2/3 precommits for Block at Round were found
  p2p::tstamp commit_time;
  std::shared_ptr<validator_set> validators;
  std::optional<p2p::proposal_message> proposal;
  std::optional<block> proposal_block;
  std::optional<part_set> proposal_block_parts;
  int32_t locked_round;
  std::optional<block> locked_block;
  std::optional<part_set> locked_block_parts;

  // Last known round with POL for non-nil valid block.
  int32_t valid_round;
  std::optional<block> valid_block; // Last known block of POL mentioned above.

  std::optional<part_set> valid_block_parts;
  std::shared_ptr<height_vote_set> votes;
  int32_t commit_round;
  std::optional<vote_set> last_commit;
  std::shared_ptr<validator_set> last_validators;
  bool triggered_timeout_precommit;
};

struct timeout_info {
  std::chrono::system_clock::duration duration_;
  int64_t height;
  int32_t round;
  round_step_type step;
};

using consensus_message = std::variant<p2p::proposal_message, p2p::block_part_message, p2p::vote_message>;

struct msg_info {
  consensus_message msg;
  p2p::node_id peer_id;
};

using timeout_info_ptr = std::shared_ptr<timeout_info>;
using msg_info_ptr = std::shared_ptr<msg_info>;

namespace channels {
  using timeout_ticker = appbase::channel_decl<struct timeout_ticker_tag, timeout_info_ptr>;
  using internal_message_queue = appbase::channel_decl<struct internal_message_queue_tag, msg_info_ptr>;
  using peer_message_queue = appbase::channel_decl<struct peer_message_queue_tag, msg_info_ptr>;
} // namespace channels

inline p2p::tstamp get_time() {
  return std::chrono::system_clock::now().time_since_epoch().count();
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

/// \brief SignedHeader is a header along with the commits that prove it.
struct signed_header {
  noir::consensus::block_header header;
  std::optional<noir::consensus::commit> commit;
};

} // namespace noir::consensus

FC_REFLECT(noir::consensus::timeout_info, (duration_)(height)(round)(step))
FC_REFLECT(std::chrono::system_clock::duration, )
FC_REFLECT(noir::consensus::round_step_type, )
