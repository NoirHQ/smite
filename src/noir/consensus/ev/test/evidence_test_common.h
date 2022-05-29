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
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  return new_mock_duplicate_vote_evidence_with_validator(height, time, chain_id, val);
}

inline tstamp get_default_evidence_time() {
  std::tm tm = {
    .tm_sec = 0,
    .tm_min = 0,
    .tm_hour = 0,
    .tm_mday = 1,
    .tm_mon = 1 - 1,
    .tm_year = 2019 - 1900,
  }; /// 2019-1-1 0:0:0
  tm.tm_isdst = -1;
  auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

inline p2p::block_id make_block_id(bytes hash, uint32_t part_set_size, bytes part_set_hash) {
  return {.hash = std::move(hash), .parts = {.total = part_set_size, .hash = std::move(part_set_hash)}};
}

inline std::shared_ptr<vote> make_vote(mock_pv& val,
  const std::string& chain_id,
  int32_t val_index,
  int64_t height,
  int32_t round,
  int step,
  const p2p::block_id& block_id_,
  tstamp time) {
  auto pub_key = val.get_pub_key();
  auto ret = std::make_shared<vote>();
  ret->validator_address = pub_key.address();
  ret->validator_index = val_index;
  ret->height = height;
  ret->round = round;
  ret->type = noir::p2p::Prevote;
  ret->block_id_ = block_id_;
  ret->timestamp = time;
  val.sign_vote(*ret);
  return ret;
}

} // namespace noir::consensus::ev
