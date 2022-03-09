// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/validator.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

std::optional<std::string> verify_basic_vals_and_commit(const std::shared_ptr<validator_set>& vals,
  std::shared_ptr<struct commit> commit_, int64_t height, p2p::block_id block_id_);

/// \brief check all signatures included in a commit
std::optional<std::string> verify_commit_single(std::string chain_id_, std::shared_ptr<validator_set> vals,
  std::shared_ptr<struct commit> commit_, int64_t voting_power_needed, bool count_all_signatures, bool lookup_by_index);

/// \brief verifies +2/3 of set has signed given commit
/// Used by the light client and does not check all signatures
std::optional<std::string> verify_commit_light(std::string chain_id_, std::shared_ptr<validator_set> vals,
  p2p::block_id block_id_, int64_t height, std::shared_ptr<struct commit> commit_);

} // namespace noir::consensus
