// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/block.h>
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/validation.h>
#include <noir/consensus/vote.h>
#include <noir/core/codec.h>

namespace noir::consensus {

std::optional<std::string> verify_basic_vals_and_commit(const std::shared_ptr<validator_set>& vals,
  std::shared_ptr<commit> commit_,
  int64_t height,
  p2p::block_id block_id_) {
  if (!vals)
    return "verification failed: validator_set is not set";
  if (!commit_)
    return "verification failed: commit is not set";
  if (vals->size() != commit_->signatures.size())
    return "verification failed: not enough signatures prepared";
  if (height != commit_->height)
    return "verification failed: incorrect height";
  if (block_id_ != commit_->my_block_id)
    return "verification failed: wrong block_id";
  return {};
}

/// \brief check all signatures included in a commit
std::optional<std::string> verify_commit_single(std::string chain_id_,
  std::shared_ptr<validator_set> vals,
  std::shared_ptr<commit> commit_,
  int64_t voting_power_needed,
  bool count_all_signatures,
  bool lookup_by_index) {

  validator val{};
  int32_t val_idx{0};
  int64_t tallied_voting_power{0};
  std::map<int32_t, int> seen_vals;
  bytes vote_sign_bytes;

  for (auto i = 0; i < commit_->signatures.size(); i++) {
    auto& commit_sig_ = commit_->signatures[i];
    if (!commit_sig_.for_block())
      continue;

    if (lookup_by_index) {
      val = vals->validators[i];
    } else {
      auto val_opt = vals->get_by_address(commit_sig_.validator_address);
      if (!val_opt.has_value())
        continue;
      val = val_opt.value();
      auto val_index = vals->get_index_by_address(commit_sig_.validator_address);

      // Check if same validator committed twice
      if (auto it = seen_vals.find(val_index); it != seen_vals.end()) {
        auto second_index = i;
        return "verification failed: double vote detected";
      }
      seen_vals[val_index] = i;
    }

    auto vote_ = commit_->get_vote(i);
    vote_->block_id_ = commit_->my_block_id; // TODO: is this right? [Sam: added 20220313]
    vote_sign_bytes = encode(canonical::canonicalize_vote(*vote_));
    if (!val.pub_key_.verify_signature(vote_sign_bytes, commit_sig_.signature))
      return fmt::format("verification failed: wrong signature - index={}", i);

    tallied_voting_power += val.voting_power;
    if (!count_all_signatures && tallied_voting_power > voting_power_needed)
      return {};
  }

  if (tallied_voting_power <= voting_power_needed)
    return "verification failed: not enough votes were signed";
  return {};
}

/// \brief verifies +2/3 of set has signed given commit
/// Used by the light client and does not check all signatures
std::optional<std::string> verify_commit_light(std::string chain_id_,
  std::shared_ptr<validator_set> vals,
  p2p::block_id block_id_,
  int64_t height,
  std::shared_ptr<commit> commit_) {
  // Validate params
  if (auto err = verify_basic_vals_and_commit(vals, commit_, height, block_id_); err.has_value())
    return err;

  // Calculate required voting power
  auto voting_power_needed = vals->total_voting_power * 2 / 3;

  // Batch verification // TODO: is this required?

  // Single verification
  return verify_commit_single(chain_id_, vals, commit_, voting_power_needed, false, true);
}

} // namespace noir::consensus
