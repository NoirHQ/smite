// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/consensus/block.h>
#include <noir/consensus/vote.h>
#include <fmt/format.h>

#include <noir/codec/scale.h>

namespace noir::consensus {

std::shared_ptr<part_set> block::make_part_set(uint32_t part_size) {
  std::lock_guard<std::mutex> g(mtx);
  auto bz = codec::scale::encode(*this);
  return part_set::new_part_set_from_data(bz, part_size);
}

std::shared_ptr<vote> commit::get_vote(int32_t val_idx) {
  auto commit_sig = signatures[val_idx];
  auto ret = std::make_shared<vote>();
  ret->type = noir::p2p::signed_msg_type::Precommit;
  ret->height = height;
  ret->round = round;
  ret->block_id_ = commit_sig.get_block_id(my_block_id);
  ret->timestamp = commit_sig.timestamp;
  ret->validator_address = commit_sig.validator_address;
  ret->validator_index = val_idx;
  ret->signature = commit_sig.signature;
  ret->vote_extension_ = {
    .app_data_to_sign = commit_sig.vote_extension.add_data_to_sign}; // FIXME: app_data_self_authenticating is missing

  return std::move(ret);
}

std::shared_ptr<vote_set> commit_to_vote_set(
  std::string& chain_id, commit& commit_, validator_set& val_set) { // FIXME: add const keyword
  auto vote_set_ =
    vote_set::new_vote_set(chain_id, commit_.height, commit_.round, p2p::signed_msg_type::Precommit, val_set);
  check(vote_set_ != nullptr);
  for (auto it = commit_.signatures.begin(); it != commit_.signatures.end(); ++it) {
    auto index = std::distance(commit_.signatures.begin(), it);
    if (it->absent()) {
      continue; // OK, some precommits can be missing.
    }

    std::optional<vote> opt_{std::nullopt};
    auto vote_ = commit_.get_vote(static_cast<int32_t>(index));
    if (vote_) {
      opt_ = {*std::move(vote_)};
    }
    check(vote_set_->add_vote(opt_), "Failed to reconstruct LastCommit");
  }
  return std::move(vote_set_);
}

} // namespace noir::consensus
