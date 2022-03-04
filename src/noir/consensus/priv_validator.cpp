// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/priv_validator.h>
#include <noir/core/codec.h>

namespace noir::consensus {

std::optional<std::string> priv_validator::sign_vote(vote& vote_) {
  // TODO: add some validation checks

  auto bz = encode(vote_);
  auto sig = priv_key_.sign(bz);
  // TODO: save_signed; why?
  vote_.signature = sig;
  return {};
}

std::optional<std::string> priv_validator::sign_proposal(proposal& proposal_) {
  // TODO: add some validation checks

  auto bz = encode(proposal_);
  auto sig = priv_key_.sign(bz);
  // TODO: save_signed; why?
  proposal_.signature = sig;
  return {};
}

} // namespace noir::consensus
