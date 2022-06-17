// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/types/priv_validator.h>
#include <noir/consensus/types/proposal.h>
#include <noir/core/codec.h>

namespace noir::consensus {

std::optional<std::string> mock_pv::sign_vote(vote& vote_) {
  // TODO: add some validation checks

  auto vote_sign_bytes = vote::vote_sign_bytes("", *vote::to_proto(vote_));
  auto sig = priv_key_.sign(vote_sign_bytes);
  vote_.signature = sig;
  return {};
}

std::optional<std::string> mock_pv::sign_proposal(const std::string& chain_id, noir::p2p::proposal_message& msg) {
  // TODO: add some validation checks

  auto sign_bytes = proposal::proposal_sign_bytes(chain_id, *proposal::to_proto({msg}));
  auto sig = priv_key_.sign(sign_bytes);
  msg.signature = sig;
  return {};
}

Result<Bytes> mock_pv::sign_vote_pb(const std::string& chain_id, const ::tendermint::types::Vote& v) {
  auto sign_bytes = vote::vote_sign_bytes("" /*chain_id*/, v);
  auto sig = priv_key_.sign(sign_bytes);
  return sig;
}

} // namespace noir::consensus
