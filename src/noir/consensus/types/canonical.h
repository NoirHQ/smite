// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/vote.h>

namespace noir::consensus {

struct canonical_vote {
  p2p::signed_msg_type type;
  int64_t height{};
  int32_t round{};
  p2p::block_id block_id_;
  p2p::tstamp timestamp{};
};

struct canonical_proposal {
  p2p::signed_msg_type type;
  int64_t height{};
  int32_t round{};
  int32_t pol_round{};
  p2p::block_id block_id_;
  p2p::tstamp timestamp{};
};

struct canonical {

  static canonical_vote canonicalize_vote(const vote& vote_) {
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
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::canonical_vote, type, height, round, block_id_, timestamp);
NOIR_REFLECT(noir::consensus::canonical_proposal, type, height, round, pol_round, block_id_, timestamp);
