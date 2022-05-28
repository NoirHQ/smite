// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/vote.h>
#include <tendermint/types/canonical.pb.h>

namespace noir::consensus {

struct canonical_vote {
  p2p::signed_msg_type type;
  int64_t height{};
  int32_t round{};
  p2p::block_id block_id_;
  tstamp timestamp{};
};

struct canonical_proposal {
  p2p::signed_msg_type type;
  int64_t height{};
  int32_t round{};
  int32_t pol_round{};
  p2p::block_id block_id_;
  tstamp timestamp{};
};

struct canonical {

  static canonical_vote canonicalize_vote(const vote& vote_) { // TODO: add chain_id
    return {.type = vote_.type,
      .height = vote_.height,
      .round = vote_.round,
      .block_id_ = vote_.block_id_,
      .timestamp = vote_.timestamp};
  }

  static canonical_proposal canonicalize_proposal(const p2p::proposal_message& proposal_) {
    return {.type = proposal_.type,
      .height = proposal_.height,
      .round = proposal_.round,
      .pol_round = proposal_.pol_round,
      .block_id_ = proposal_.block_id_,
      .timestamp = proposal_.timestamp};
  }

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
    if (auto b = canonicalize_block_id(v.block_id()); !b)
      ret.set_allocated_block_id(b.release());
    *ret.mutable_timestamp() = v.timestamp();
    ret.set_chain_id(chain_id);
    return ret;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::canonical_vote, type, height, round, block_id_, timestamp);
NOIR_REFLECT(noir::consensus::canonical_proposal, type, height, round, pol_round, block_id_, timestamp);
