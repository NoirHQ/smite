// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/proposal.h>

namespace noir::consensus {

Bytes proposal::proposal_sign_bytes(const std::string& chain_id, const ::tendermint::types::Proposal& p) {
  auto proposal_pb = canonical::canonicalize_proposal_pb(chain_id, p);
  Bytes sign_bytes(proposal_pb.ByteSizeLong());
  proposal_pb.SerializeToArray(sign_bytes.data(), proposal_pb.ByteSizeLong());
  return sign_bytes;
}

} // namespace noir::consensus
