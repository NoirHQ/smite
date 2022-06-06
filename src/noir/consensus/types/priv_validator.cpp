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

std::optional<std::string> mock_pv::sign_proposal(noir::p2p::proposal_message& proposal_) {
  // TODO: add some validation checks

  auto proposal_sign_bytes = encode(canonical::canonicalize_proposal(proposal_));
  auto sig = priv_key_.sign(proposal_sign_bytes);
  proposal_.signature = sig;
  return {};
}

Result<bytes> mock_pv::sign_vote_pb(const std::string& chain_id, const ::tendermint::types::Vote& v) {
  auto sign_bytes = vote::vote_sign_bytes(chain_id, v);
  auto sig = priv_key_.sign(sign_bytes);
  return sig;
}

} // namespace noir::consensus
