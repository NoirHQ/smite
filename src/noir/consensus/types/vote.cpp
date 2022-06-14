// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/bit_array.h>
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/vote.h>
#include <noir/core/codec.h>

namespace noir::consensus {

Bytes vote::vote_sign_bytes(const std::string& chain_id, const ::tendermint::types::Vote& v) {
  auto pb = canonical::canonicalize_vote_pb(chain_id, v);
  Bytes sign_bytes(pb.ByteSizeLong());
  pb.SerializeToArray(sign_bytes.data(), pb.ByteSizeLong());
  return sign_bytes;
}

std::shared_ptr<vote_set> vote_set::new_vote_set(const std::string& chain_id_,
  int64_t height_,
  int32_t round_,
  p2p::signed_msg_type signed_msg_type,
  const std::shared_ptr<validator_set>& val_set_) {
  if (height_ == 0)
    throw std::runtime_error("Cannot make vote_set for height = 0, doesn't make sense");
  auto ret = std::make_shared<vote_set>();
  ret->chain_id = chain_id_;
  ret->height = height_;
  ret->round = round_;
  ret->signed_msg_type_ = signed_msg_type;
  auto new_val_set = validator_set::new_validator_set({});
  new_val_set = val_set_;
  ret->val_set = new_val_set;
  ret->votes_bit_array = bit_array::new_bit_array(val_set_->size());
  ret->votes.resize(val_set_->size());
  ret->sum = 0;
  return ret;
}

std::shared_ptr<bit_array> vote_set::get_bit_array() {
  std::scoped_lock g(mtx);
  return votes_bit_array->copy();
}

std::pair<bool, Error> vote_set::add_vote(std::optional<vote> vote_) {
  if (!vote_)
    check(false, "add_vote() on empty vote_set");
  std::scoped_lock g(mtx);

  auto val_index = vote_->validator_index;
  auto val_addr = vote_->validator_address;
  auto block_key = vote_->block_id_.key();

  // Ensure that validator index is set
  if (val_index < 0)
    return {false, Error::format("index < 0: {}", ErrVoteInvalidValidatorIndex.message())};
  if (val_addr.empty())
    return {false, Error::format("empty address: {}", ErrVoteInvalidValidatorAddress.message())};

  // Make sure step matches
  if ((vote_->height != height) || (vote_->round != round) || (vote_->type != signed_msg_type_)) {
    return {false,
      Error::format("expected {}/{}/{} but got {}/{}/{}", vote_->height, vote_->round, vote_->type, height, round,
        signed_msg_type_)};
  }

  // Ensure that signer is a validator
  auto val = val_set->get_by_index(val_index);
  if (!val)
    return {
      false, Error::format("cannot find validator {} in val_set of size {}", val_index, val_set->validators.size())};

  // Ensure that signer has the right address
  if (val_addr != val->address)
    return {false, Error::format("signer has wrong address")};

  // Check if the same vote exists
  if (auto existing = get_vote(val_index, block_key); existing) {
    if (existing->signature == vote_->signature) {
      // duplicate
      return {false, Error{}};
    }
    elog("same vote exists");
    return {false, ErrVoteNonDeterministicSignature};
  }

  // Check signature
  if (val->pub_key_.address() != val_addr)
    return {false, Error::format("invalid validator address")};
  auto vote_sign_bytes_ = encode(canonical::canonicalize_vote(*vote_));
  if (!val->pub_key_.verify_signature(vote_sign_bytes_, vote_->signature))
    return {false, Error::format("invalid signature")};

  // Add vote and get conflicting vote if any
  auto voting_power = val->voting_power;
  std::optional<vote> conflicting;

  // Already exists in vote_set.votes?
  if (votes.size() > 0 && votes.size() >= val_index && votes[val_index]) {
    auto& existing = votes[val_index];
    if (existing->block_id_ == vote_->block_id_) {
      check(false, "add_vote() does not expect duplicate votes");
    } else {
      conflicting = existing;
    }
    // Replace vote if block_key matches vote_set.maj23
    if (maj23.has_value() && maj23.value().key() == block_key) {
      votes[val_index] = vote_;
      votes_bit_array->set_index(val_index, true);
    }
    // Otherwise, don't add to vote_set.votes
  } else {
    // Add to vote_set.votes and increase sum
    votes[val_index] = vote_;
    votes_bit_array->set_index(val_index, true);
    sum += voting_power;
  }

  std::shared_ptr<block_votes> new_votes_by_block;
  if (votes_by_block.contains(block_key)) {
    new_votes_by_block = votes_by_block[block_key];
    if (conflicting && votes_by_block[block_key]->peer_maj23) {
      // There's a conflict and no peer claims that this block is special.
      return {false, NewConflictingVoteError};
    }
    // We'll add the vote in a bit.
  } else {
    if (conflicting) {
      // there's a conflicting vote.
      // We're not even tracking this blockKey, so just forget it.
      return {false, NewConflictingVoteError};
    }
    // Start tracking this blockKey
    new_votes_by_block = block_votes::new_block_votes(false, val_set->size());
    votes_by_block[block_key] = new_votes_by_block;
    // We'll add the vote in a bit.
  }

  // Before adding to votesByBlock, see if we'll exceed quorum
  auto orig_sum = new_votes_by_block->sum;
  auto quorum = val_set->get_total_voting_power() * 2 / 3 + 1;

  // Add vote to votesByBlock
  new_votes_by_block->add_verified_vote(vote_.value(), voting_power);

  // If we just crossed the quorum threshold and have 2/3 majority...
  if (orig_sum < quorum && quorum <= new_votes_by_block->sum) {
    // Only consider the first quorum reached
    if (!maj23.has_value()) {
      auto maj23_block_id = vote_->block_id_;
      maj23 = maj23_block_id;
      // And also copy votes over to voteSet.votes
      votes = new_votes_by_block->votes;
    }
  }

  return {true, Error{}};
}

} // namespace noir::consensus
