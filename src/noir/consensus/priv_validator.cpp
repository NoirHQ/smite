// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/scale.h>
#include <noir/consensus/priv_validator.h>

namespace noir::consensus {

std::optional<std::string> priv_validator::sign_vote(vote& vote_) {
  // TODO: add some validation checks

  // auto bz = codec::scale::encode(vote_);
  auto bz = codec::scale::encode(static_cast<p2p::vote_message>(vote_));
  auto sig = priv_key_.sign(bz);
  // TODO: save_signed; why?
  vote_.signature = sig;
  return {};
}

std::optional<std::string> priv_validator::sign_proposal(proposal& proposal_) {
  // TODO: add some validation checks

  // auto bz = codec::scale::encode(proposal_);
  auto bz = codec::scale::encode(static_cast<p2p::proposal_message>(proposal_));
  auto sig = priv_key_.sign(bz);
  // TODO: save_signed; why?
  proposal_.signature = sig;
  return {};
}

} // namespace noir::consensus
