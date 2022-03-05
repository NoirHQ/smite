// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block.h>
#include <noir/consensus/validator.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

#include <fmt/core.h>

namespace noir::consensus {

using P2PID = std::string;

/**
 * represents a prevote, precommit, or commit vote from validators for consensus
 */
struct vote : p2p::vote_message {

  commit_sig to_commit_sig() {
    block_id_flag flag;
    if (block_id_.is_complete())
      flag = FlagCommit;
    else if (block_id_.is_zero())
      flag = FlagNil;
    else
      throw std::runtime_error(fmt::format("Invalid vote - expected block_id to be either empty or complete"));
    return commit_sig{flag, validator_address, timestamp, signature, vote_extension_.to_sign()};
  }
};

struct block_votes {
  bool peer_maj23{};
  std::shared_ptr<bit_array> bit_array_;
  std::vector<vote> votes;
  int64_t sum{};

  static block_votes new_block_votes(bool peer_maj23_, int num_validators) {
    block_votes ret{peer_maj23_};
    ret.bit_array_ = bit_array::new_bit_array(num_validators);
    ret.votes.resize(num_validators);
    return ret;
  }

  void add_verified_vote(const vote& vote_, int64_t voting_power) {
    auto val_index = vote_.validator_index;
    if (!votes.empty() && votes.size() >= val_index) {
      auto existing = votes[val_index];
      bit_array_->set_index(val_index, true);
      votes[val_index] = vote_;
      sum += voting_power;
    }
  }

  std::optional<vote> get_by_index(int32_t index) {
    if (!votes.empty() && votes.size() >= index)
      return votes[index];
    return {};
  }
};

/**
 * VoteSet helps collect signatures from validators at each height+round for a
 * predefined vote type.
 *
 * We need VoteSet to be able to keep track of conflicting votes when validators
 * double-sign.  Yet, we can't keep track of *all* the votes seen, as that could
 * be a DoS attack vector.
 *
 * There are two storage areas for votes.
 * 1. voteSet.votes
 * 2. voteSet.votesByBlock
 *
 * `.votes` is the "canonical" list of votes.  It always has at least one vote,
 * if a vote from a validator had been seen at all.  Usually it keeps track of
 * the first vote seen, but when a 2/3 majority is found, votes for that get
 * priority and are copied over from `.votesByBlock`.
 *
 * `.votesByBlock` keeps track of a list of votes for a particular block.  There
 * are two ways a &blockVotes{} gets created in `.votesByBlock`.
 * 1. the first vote seen by a validator was for the particular block.
 * 2. a peer claims to have seen 2/3 majority for the particular block.
 *
 * Since the first vote from a validator will always get added in `.votesByBlock`
 * , all votes in `.votes` will have a corresponding entry in `.votesByBlock`.
 *
 * When a &blockVotes{} in `.votesByBlock` reaches a 2/3 majority quorum, its
 * votes are copied into `.votes`.
 *
 * All this is memory bounded because conflicting votes only get added if a peer
 * told us to track that block, each peer only gets to tell us 1 such block, and,
 * there's only a limited number of peers.
 *
 * NOTE: Assumes that the sum total of voting power does not exceed MaxUInt64.
 */
struct vote_set {
  std::string chain_id;
  int64_t height;
  int32_t round;
  p2p::signed_msg_type signed_msg_type_;
  validator_set val_set;

  std::mutex mtx;
  std::shared_ptr<bit_array> votes_bit_array{};
  std::vector<vote> votes;
  int64_t sum;
  std::optional<p2p::block_id> maj23;
  std::map<std::string, block_votes> votes_by_block;
  std::map<P2PID, p2p::block_id> peer_maj23s;

  static std::shared_ptr<vote_set> new_vote_set(std::string& chain_id_, int64_t height_, int32_t round_,
    p2p::signed_msg_type signed_msg_type, validator_set& val_set_);

  std::shared_ptr<bit_array> get_bit_array();

  virtual int get_size() const {
    return val_set.size();
  }

  bool add_vote(std::optional<vote> vote_);

  std::optional<vote> get_vote(int32_t val_index, const std::string& block_key) {
    if (votes.size() > 0 && votes.size() >= val_index) {
      auto existing = votes[val_index];
      if (existing.block_id_.key() == block_key)
        return existing;
    }
    if (votes_by_block.contains(block_key)) {
      auto existing = votes_by_block[block_key].get_by_index(val_index);
      if (existing.has_value())
        return existing;
    }
    return {};
  }

  std::shared_ptr<bit_array> bit_array_by_block_id(p2p::block_id block_id_) {
    std::lock_guard<std::mutex> g(mtx);
    auto it = votes_by_block.find(block_id_.key());
    if (it != votes_by_block.end())
      return it->second.bit_array_->copy();
    return {};
  }

  std::optional<std::string> set_peer_maj23(std::string peer_id, p2p::block_id block_id_) {
    std::lock_guard<std::mutex> g(mtx);

    auto block_key = block_id_.key();

    // Make sure peer has not sent us something yet
    if (auto it = peer_maj23s.find(peer_id); it != peer_maj23s.end()) {
      if (it->second == block_id_)
        return {}; // nothing to do
      return "setPeerMaj23: Received conflicting blockID";
    }
    peer_maj23s[peer_id] = block_id_;

    // Create votes_by_block
    auto it = votes_by_block.find(block_key);
    if (it != votes_by_block.end()) {
      if (it->second.peer_maj23)
        return {}; // nothing to do
      it->second.peer_maj23 = true;
    } else {
      auto new_votes_by_block = block_votes::new_block_votes(true, val_set.size());
      votes_by_block[block_key] = new_votes_by_block;
    }
    return {};
  }

  bool has_two_thirds_majority() {
    std::lock_guard<std::mutex> g(mtx);
    return maj23.has_value();
  }

  bool has_two_thirds_any() {
    std::lock_guard<std::mutex> g(mtx);
    return sum > val_set.total_voting_power * 2 / 3;
  }

  bool has_all() {
    std::lock_guard<std::mutex> g(mtx);
    return sum == val_set.total_voting_power;
  }

  /**
   * if there is a 2/3+ majority for block_id, return block_id
   */
  std::optional<p2p::block_id> two_thirds_majority() {
    std::lock_guard<std::mutex> g(mtx);
    // if (maj23.has_value())
    //  return maj23;
    // return {};
    return maj23;
  }

  /**
   * constructs a commit from the vote_set. It only include precommits for the block, which has 2/3+ majority and nil
   */
  commit make_commit() {
    std::lock_guard<std::mutex> g(mtx);
    if (signed_msg_type_ != p2p::Precommit)
      throw std::runtime_error("cannot make_commit() unless signed_msg_type_ is Precommit");
    // Make sure we have a 2/3 majority
    if (!maj23.has_value())
      throw std::runtime_error("cannot make_comit() unless a block has 2/3+");
    // For every validator, get the precommit
    std::vector<commit_sig> commit_sigs;
    for (auto& vote : votes) {
      auto commit_sig_ = vote.to_commit_sig();
      // If block_id exists but does not match, exclude sig
      if (commit_sig_.for_block() && (vote.block_id_ != maj23)) {
        commit_sig_ = commit_sig::new_commit_sig_absent();
      }
      commit_sigs.push_back(commit_sig_);
    }
    return commit::new_commit(height, round, maj23.value(), commit_sigs);
  }
};

struct nil_vote_set : vote_set {
  nil_vote_set() {
    height = 0;
    round = -1;
    signed_msg_type_ = p2p::signed_msg_type::Unknown;
  }
  int get_size() const override {
    return 0;
  }
};

struct vote_set_reader {
  int64_t height;
  int32_t round;
  std::shared_ptr<bit_array> bit_array_;
  bool is_commit;
  p2p::signed_msg_type type;
  int size;
  std::vector<vote> votes;

  vote_set_reader() = delete;
  vote_set_reader(const vote_set_reader&) = delete;
  vote_set_reader(vote_set_reader&&) = default;
  vote_set_reader& operator=(const vote_set_reader&) = delete;

  explicit vote_set_reader(const commit& commit_) {
    height = commit_.height;
    round = commit_.round;
    bit_array_ = commit_.bit_array_;
    is_commit = commit_.signatures.size() != 0;
    type = p2p::Precommit;
    size = commit_.signatures.size();
    int i{0};
    for (auto sig : commit_.signatures) {
      votes.push_back(vote{p2p::Precommit, commit_.height, commit_.round, sig.get_block_id(commit_.my_block_id),
        sig.timestamp, sig.validator_address, i++, sig.signature});
    }
  }
  explicit vote_set_reader(const vote_set& vote_set_) {
    height = vote_set_.height;
    round = vote_set_.round;
    bit_array_ = vote_set_.votes_bit_array;
    if (vote_set_.signed_msg_type_ != p2p::Precommit)
      is_commit = false;
    else
      is_commit = vote_set_.maj23.has_value();
    type = vote_set_.signed_msg_type_;
    size = vote_set_.val_set.size();
    votes = vote_set_.votes;
  }

  std::shared_ptr<vote> get_by_index(int32_t val_index) {
    if (votes.empty())
      return nullptr;
    if (val_index >= votes.size())
      return nullptr;
    return std::make_shared<vote>(votes[val_index]);
  }
};

} // namespace noir::consensus

NOIR_REFLECT_DERIVED(noir::consensus::vote, noir::p2p::vote_message)
