// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/types/validator.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

noir::Result<void> verify_basic_vals_and_commit(const std::shared_ptr<validator_set>& vals,
  std::shared_ptr<struct commit> commit_,
  int64_t height,
  p2p::block_id block_id_);

/// \brief check all signatures included in a commit
noir::Result<void> verify_commit_single(const std::string& chain_id_,
  const std::shared_ptr<validator_set>& vals,
  const std::shared_ptr<struct commit>& commit_,
  int64_t voting_power_needed,
  bool count_all_signatures,
  bool lookup_by_index);

/// \brief verifies +2/3 of set has signed given commit
/// Used by the light client and does not check all signatures
noir::Result<void> verify_commit_light(const std::string& chain_id_,
  const std::shared_ptr<validator_set>& vals,
  const p2p::block_id& block_id_,
  int64_t height,
  const std::shared_ptr<struct commit>& commit_);

Result<void> verify_commit_light_trusting(const std::string& chain_id_,
  const std::shared_ptr<validator_set>& vals,
  const std::shared_ptr<struct commit>& commit_);

} // namespace noir::consensus
