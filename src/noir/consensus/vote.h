// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma
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
  // bitarray
  std::vector<vote> votes;
  int64_t sum{};

  static block_votes new_block_votes(bool peer_maj23_, int num_validators) {
    block_votes ret{peer_maj23_};
    ret.votes.resize(num_validators);
    return ret;
  }

  void add_verified_vote(const vote& vote_, int64_t voting_power) {
    auto val_index = vote_.validator_index;
    if (votes.size() <= val_index) {
      auto existing = votes[val_index];
      // vs.bitArray.SetIndex(int(valIndex), true) // todo - bit_array
      votes[val_index] = vote_;
      sum += voting_power;
    }
  }

  std::optional<vote> get_by_index(int32_t index) {
    if (votes.size() <= index)
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

  //  std::mutex mtx;
  // bit_array
  std::vector<vote> votes;
  int64_t sum;
  std::optional<p2p::block_id> maj23;
  std::map<std::string, block_votes> votes_by_block;
  std::map<P2PID, p2p::block_id> peer_maj23s;

  static vote_set new_vote_set(std::string chain_id, int64_t height, int32_t round,
    p2p::signed_msg_type signed_msg_type_, validator_set val_set_) {
    if (height == 0)
      throw std::runtime_error("Cannot make vote_set for height = 0, doesn't make sense");
    auto ret = vote_set{chain_id, height, round, signed_msg_type_, val_set_};
    ret.sum = 0;
    return ret;
  }

  bool add_vote(std::optional<vote> vote_) {
    if (!vote_.has_value())
      throw std::runtime_error("add_vote() on empty vote_set");

    auto val_index = vote_->validator_index;
    auto val_addr = vote_->validator_address;
    auto block_key = vote_->block_id_.key();

    // Ensure that validator index is set
    if (val_index < 0) {
      elog("add_vote(): validator index < 0");
      return false;
    }
    if (val_addr.empty()) {
      elog("add_vote(): invalid validator address");
      return false;
    }

    // Make sure step matches
    if ((vote_->height != height) || (vote_->round != round) || (vote_->type != signed_msg_type_)) {
      elog(fmt::format("expected {}/{}/{} but got {}/{}/{}", vote_->height, vote_->round, vote_->type, height, round,
        signed_msg_type_));
      return false;
    }

    // Ensure that signer is a validator
    auto val = val_set.get_by_index(val_index);
    if (!val.has_value()) {
      elog(fmt::format("cannot found validator {} in val_set of size {}", val_index, val_set.validators.size()));
      return false;
    }

    // Ensure that signer has the right address
    if (val_addr != val->address) {
      elog("add_vote() failed: signer has wrong addres");
      return false;
    }

    // Check if the same vote exists
    if (auto existing = get_vote(val_index, block_key); existing.has_value()) {
      if (existing->signature == vote_->signature) {
        // duplicate
        return false;
      }
      elog("add_vote() failed: same vote exists");
      return false;
    }

    // Check signature
    // todo

    // Add vote and get conflicting vote if any
    // todo - directly implement addVerifiedVote() here
    auto voting_power = val->voting_power;
    std::optional<vote> conflicting;

    // Already exists in vote_set.votes?
    if (votes.size() > 0 && votes.size() >= val_index) {
      auto existing = votes[val_index];
      if (existing.block_id_ == vote_->block_id_) {
        throw std::runtime_error("add_vote() does not expect duplicate votes");
      } else {
        conflicting = existing;
      }
      // Replace vote if block_key matches vote_set.maj23
      if (maj23.has_value() && maj23.value().key() == block_key) {
        votes[val_index] = vote_.value();
        // voteSet.votesBitArray.SetIndex(int(valIndex), true) // todo - bit_array
      }
      // Otherwise, don't add to vote_set.votes
    } else {
      // Add to vote_set.votes and increase sum
      votes.push_back(vote_.value());
      // voteSet.votesBitArray.SetIndex(int(valIndex), true) // todo - bit_array
      sum += voting_power;
    }

    block_votes new_votes_by_block;
    if (votes_by_block.contains(block_key)) {
      new_votes_by_block = votes_by_block[block_key];
      if (conflicting.has_value() && votes_by_block[block_key].peer_maj23) {
        // There's a conflict and no peer claims that this block is special.
        return false;
      }
      // We'll add the vote in a bit.
    } else {
      if (conflicting.has_value()) {
        // there's a conflicting vote.
        // We're not even tracking this blockKey, so just forget it.
        return false;
      }
      // Start tracking this blockKey
      new_votes_by_block = block_votes::new_block_votes(false, val_set.size());
      votes_by_block[block_key] = new_votes_by_block;
      // We'll add the vote in a bit.
    }

    // Before adding to votesByBlock, see if we'll exceed quorum
    auto orig_sum = new_votes_by_block.sum;
    auto quorum = val_set.get_total_voting_power() * 2 / 3 + 1;

    // Add vote to votesByBlock
    new_votes_by_block.add_verified_vote(vote_.value(), voting_power);

    // If we just crossed the quorum threshold and have 2/3 majority...
    if (orig_sum < quorum && quorum <= new_votes_by_block.sum) {
      // Only consider the first quorum reached
      if (!maj23.has_value()) {
        auto maj23_block_id = vote_.value().block_id_;
        maj23 = maj23_block_id;
        // And also copy votes over to voteSet.votes
        for (auto i = 0; const auto& v : new_votes_by_block.votes) {
          votes[i++] = v;
        }
      }
    }

    return true;
  }

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

  bool has_two_thirds_majority() {
    // todo - use mtx
    return maj23.has_value();
  }

  bool has_two_thirds_any() {
    // todo - use mtx
    return sum > val_set.total_voting_power * 2 / 3;
  }

  bool has_all() {
    // todo - use mtx
    return sum == val_set.total_voting_power;
  }

  /**
   * if there is a 2/3+ majority for block_id, return block_id
   */
  std::optional<p2p::block_id> two_thirds_majority() {
    // todo - use mtx
    // if (maj23.has_value())
    //  return maj23;
    // return {};
    return maj23;
  }

  /**
   * constructs a commit from the vote_set. It only include precommits for the block, which has 2/3+ majority and nil
   */
  commit make_commit() {
    // todo - use mtx
    if (signed_msg_type_ == p2p::Precommit)
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

} // namespace noir::consensus
