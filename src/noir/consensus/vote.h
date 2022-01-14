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
struct vote {
  p2p::signed_msg_type type;
  int64_t height;
  int32_t round;
  p2p::block_id block_id_;
  p2p::tstamp timestamp;
  p2p::bytes validator_address;
  int32_t validator_index;
  p2p::bytes signature;
  p2p::vote_extension vote_extension_;

  commit_sig to_commit_sig() {
    if (this == nullptr)
      return commit_sig{FlagAbsent};
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
  bool peer_maj23;
  // bitarray
  p2p::vote_message votes;
  int64_t sum;
};

/**
 * 	VoteSet helps collect signatures from validators at each height+round for a
        predefined vote type.

        We need VoteSet to be able to keep track of conflicting votes when validators
        double-sign.  Yet, we can't keep track of *all* the votes seen, as that could
        be a DoS attack vector.

        There are two storage areas for votes.
        1. voteSet.votes
        2. voteSet.votesByBlock

        `.votes` is the "canonical" list of votes.  It always has at least one vote,
        if a vote from a validator had been seen at all.  Usually it keeps track of
        the first vote seen, but when a 2/3 majority is found, votes for that get
        priority and are copied over from `.votesByBlock`.

        `.votesByBlock` keeps track of a list of votes for a particular block.  There
        are two ways a &blockVotes{} gets created in `.votesByBlock`.
        1. the first vote seen by a validator was for the particular block.
        2. a peer claims to have seen 2/3 majority for the particular block.

        Since the first vote from a validator will always get added in `.votesByBlock`
        , all votes in `.votes` will have a corresponding entry in `.votesByBlock`.

        When a &blockVotes{} in `.votesByBlock` reaches a 2/3 majority quorum, its
        votes are copied into `.votes`.

        All this is memory bounded because conflicting votes only get added if a peer
        told us to track that block, each peer only gets to tell us 1 such block, and,
        there's only a limited number of peers.

        NOTE: Assumes that the sum total of voting power does not exceed MaxUInt64.
 */
struct vote_set {
  std::string chain_id;
  int64_t height;
  int32_t round;
  p2p::signed_msg_type signed_msg_type_;
  validator_set val_set;

  //  std::mutex mtx;
  // bit_array
  p2p::vote_message votes;
  int64_t sum;
  p2p::block_id maj23;
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
    //    if (get_vote) // todo

    // Check signature
    // todo

    // Add vote and get conflicting vote if any
    // todo - directly implement addVerifiedVote() here
    return true;
  }
};

} // namespace noir::consensus
