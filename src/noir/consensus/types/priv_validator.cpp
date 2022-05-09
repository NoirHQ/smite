// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/priv_validator.h>
#include <noir/core/codec.h>

namespace noir::consensus {

std::optional<std::string> mock_pv::sign_vote(vote& vote_) {
  // TODO: add some validation checks

  auto vote_sign_bytes = encode(canonical::canonicalize_vote(vote_));
  auto sig = priv_key_.sign(vote_sign_bytes);
  vote_.signature = sig;
  return {};
}

std::optional<std::string> mock_pv::sign_proposal(proposal& proposal_) {
  // TODO: add some validation checks

  auto proposal_sign_bytes = encode(canonical::canonicalize_proposal(proposal_));
  auto sig = priv_key_.sign(proposal_sign_bytes);
  proposal_.signature = sig;
  return {};
}

} // namespace noir::consensus
