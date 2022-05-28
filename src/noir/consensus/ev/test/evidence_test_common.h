// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/priv_validator.h>

namespace noir::consensus::ev {

inline noir::bytes gen_random_bytes(size_t num) {
  noir::bytes ret(num);
  noir::crypto::rand_bytes(ret);
  return ret;
}

inline p2p::block_id rand_block_id() {
  return {.hash = gen_random_bytes(32), .parts = {.total = 1, .hash = gen_random_bytes(32)}};
}

inline std::shared_ptr<vote> make_mock_vote(
  int64_t height, int32_t round, int32_t index, bytes addr, const p2p::block_id& block_id_, tstamp time) {
  auto ret = std::make_shared<vote>();
  ret->type = noir::p2p::Precommit;
  ret->height = height;
  ret->round = round;
  ret->block_id_ = block_id_;
  ret->timestamp = time;
  ret->validator_address = addr;
  ret->validator_index = index;
  return ret;
}

inline std::shared_ptr<duplicate_vote_evidence> new_mock_duplicate_vote_evidence_with_validator(
  int64_t height, tstamp time, const std::string& chain_id, noir::consensus::priv_validator& pv) {
  auto ret = std::make_shared<duplicate_vote_evidence>();
  auto pub_key_ = pv.get_pub_key();
  auto val = validator::new_validator(pub_key_, 10);
  auto vote_a = make_mock_vote(height, 0, 0, pub_key_.address(), rand_block_id(), time);
  auto v_a = vote::to_proto(*vote_a);
  vote_a->signature = pv.sign_vote_pb(chain_id, *v_a).value();
  auto vote_b = make_mock_vote(height, 0, 0, pub_key_.address(), rand_block_id(), time);
  auto v_b = vote::to_proto(*vote_b);
  vote_b->signature = pv.sign_vote_pb(chain_id, *v_b).value();
  auto val_set = std::make_shared<validator_set>(validator_set::new_validator_set({val}));
  auto ev = duplicate_vote_evidence::new_duplicate_vote_evidence(vote_a, vote_b, time, val_set);
  return ev.value();
}

inline std::shared_ptr<duplicate_vote_evidence> new_mock_duplicate_vote_evidence(
  int64_t height, tstamp time, const std::string& chain_id) {
  auto val = mock_pv();
  return new_mock_duplicate_vote_evidence_with_validator(height, time, chain_id, val);
}

} // namespace noir::consensus::ev
