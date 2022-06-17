// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>
#include <tendermint/types/canonical.pb.h>

namespace noir::consensus {

struct canonical {
  static ::tendermint::types::CanonicalPartSetHeader canonicalize_part_set_header(
    const ::tendermint::types::PartSetHeader& p) {
    ::tendermint::types::CanonicalPartSetHeader ret;
    ret.set_total(p.total());
    ret.set_hash(p.hash());
    return ret;
  }

  static std::unique_ptr<::tendermint::types::CanonicalBlockID> canonicalize_block_id(
    const ::tendermint::types::BlockID& b) {
    auto rbid = p2p::block_id::from_proto(b);
    if (!rbid || rbid->is_zero())
      return {};
    auto ret = std::make_unique<::tendermint::types::CanonicalBlockID>();
    ret->set_hash(b.hash());
    *ret->mutable_part_set_header() = canonicalize_part_set_header(b.part_set_header());
    return ret;
  }

  static ::tendermint::types::CanonicalVote canonicalize_vote_pb(
    const std::string& chain_id, const ::tendermint::types::Vote& v) {
    ::tendermint::types::CanonicalVote ret;
    ret.set_type(v.type());
    ret.set_height(v.height());
    ret.set_round(v.round());
    if (auto b = canonicalize_block_id(v.block_id()); b)
      ret.set_allocated_block_id(b.release());
    *ret.mutable_timestamp() = v.timestamp();
    ret.set_chain_id(chain_id);
    return ret;
  }

  static ::tendermint::types::CanonicalProposal canonicalize_proposal_pb(
    const std::string& chain_id, const ::tendermint::types::Proposal& pb) {
    ::tendermint::types::CanonicalProposal ret;
    ret.set_type(tendermint::types::SIGNED_MSG_TYPE_PROPOSAL);
    ret.set_height(pb.height());
    ret.set_round(pb.round());
    ret.set_pol_round(pb.pol_round());
    if (auto b = canonicalize_block_id(pb.block_id()); b)
      ret.set_allocated_block_id(b.release());
    *ret.mutable_timestamp() = pb.timestamp();
    ret.set_chain_id(chain_id);
    return ret;
  }
};

} // namespace noir::consensus
