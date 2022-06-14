// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/node_id.h>
#include <noir/consensus/types/validator.h>
#include <noir/consensus/types/vote.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

const Error ErrGotVoteFromUnwantedRound =
  user_error_registry().register_error("peer has sent a vote that does not match our round for more than one round");

struct round_vote_set {
  std::shared_ptr<vote_set> prevotes;
  std::shared_ptr<vote_set> precommits;
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
  std::shared_ptr<validator_set> val_set{};

  std::mutex mtx;
  int32_t round;
  std::map<int32_t, round_vote_set> round_vote_sets;
  std::map<node_id, std::vector<int32_t>> peer_catchup_rounds;

  static std::shared_ptr<height_vote_set> new_height_vote_set(
    const std::string& chain_id_, int64_t height_, const std::shared_ptr<validator_set>& val_set_) {
    auto hvs = std::make_shared<height_vote_set>();
    hvs->chain_id = chain_id_;
    hvs->reset(height_, val_set_);
    return hvs;
  }

  void reset(int64_t height_, const std::shared_ptr<validator_set>& val_set_) {
    std::scoped_lock g(mtx);
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
    std::scoped_lock g(mtx);
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

  std::optional<std::string> set_peer_maj23(
    int32_t round, p2p::signed_msg_type vote_type, std::string peer_id, p2p::block_id block_id_) {
    std::scoped_lock g(mtx);
    if (vote_type != p2p::Prevote && vote_type != p2p::Precommit)
      return "setPeerMaj23: Invalid vote type";
    auto vote_set_ = get_vote_set(round, vote_type);
    if (vote_set_ == nullptr)
      return {};
    return vote_set_->set_peer_maj23(peer_id, block_id_);
  }

  std::shared_ptr<vote_set> get_vote_set(int32_t round_, p2p::signed_msg_type vote_type) {
    auto it = round_vote_sets.find(round_);
    if (it == round_vote_sets.end())
      return nullptr;
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
    std::scoped_lock g(mtx);
    for (auto r = round; r >= 0; r--) {
      auto rvs = get_vote_set(r, p2p::Prevote);
      if (rvs->two_thirds_majority().has_value())
        return r;
    }
    return -1;
  }

  std::pair<bool, Error> add_vote(const std::shared_ptr<vote>& vote_, const node_id& peer_id) {
    std::scoped_lock g(mtx);
    if (!p2p::is_vote_type_valid(vote_->type))
      return std::make_pair(false, Error{});
    auto vote_set_ = get_vote_set(vote_->round, vote_->type);
    if (vote_set_ == nullptr) {
      auto it = peer_catchup_rounds.find(peer_id);
      if (it == peer_catchup_rounds.end() || it->second.size() >= 2) {
        // punish peer
        return std::make_pair(false, ErrGotVoteFromUnwantedRound);
      }
      add_round(vote_->round);
      vote_set_ = get_vote_set(vote_->round, vote_->type);
      it->second.push_back(vote_->round);
    }
    return vote_set_->add_vote(vote_);
  }

  std::shared_ptr<vote_set> prevotes(int32_t round_) {
    std::scoped_lock g(mtx);
    return get_vote_set(round_, p2p::Prevote);
  }

  std::shared_ptr<vote_set> precommits(int32_t round_) {
    std::scoped_lock g(mtx);
    return get_vote_set(round_, p2p::Precommit);
  }
};

} // namespace noir::consensus
