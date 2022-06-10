// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/ev/evidence_pool.h>
#include <noir/consensus/store/block_store.h>
#include <noir/consensus/store/state_store.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/priv_validator.h>

namespace noir::consensus::ev {

constexpr auto evidence_chain_id = "my_chain";
constexpr auto evidence_store_path = "/tmp/ev";
constexpr auto state_store_path = "/tmp/ev_state";
constexpr auto block_store_path = "/tmp/ev_block";

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
  auto val_set = validator_set::new_validator_set({val});
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

inline std::shared_ptr<noir::consensus::db_store> initialize_state_from_validator_set(
  const std::shared_ptr<validator_set>& val_set, int64_t height) {
  auto default_evidence_time = get_default_evidence_time();
  auto state_store_ = std::make_shared<noir::consensus::db_store>(make_session(true, state_store_path));
  auto state_ = state{.chain_id = evidence_chain_id,
    .initial_height = 1,
    .last_block_height = height,
    .last_block_time = default_evidence_time,
    .validators = val_set,
    .next_validators = val_set->copy_increment_proposer_priority(1),
    .last_validators = val_set,
    .last_height_validators_changed = 1,
    .consensus_params_ = consensus_params{.block = {.max_bytes = 22020096, .max_gas = -1},
      .evidence = {.max_age_num_blocks = 20,
        .max_age_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(20)).count(),
        .max_bytes = 1000}}};
  for (auto i = 0; i <= height; i++) {
    state_.last_block_height = i;
    state_store_->save(state_);
  }
  return state_store_;
}

inline std::shared_ptr<noir::consensus::db_store> initialize_validator_state(
  const std::shared_ptr<mock_pv>& priv_val, int64_t height) {
  auto pub_key_ = priv_val->get_pub_key();
  auto val = validator::new_validator(pub_key_, 10);
  auto val_set = std::make_shared<validator_set>();
  val_set->validators.push_back(val);
  val_set->proposer = val;
  return initialize_state_from_validator_set(val_set, height);
}

inline std::shared_ptr<commit> make_commit(int64_t height, const bytes& val_addr) {
  auto default_evidence_time = get_default_evidence_time();
  auto commit_sigs = std::vector<commit_sig>();
  commit_sigs.push_back(commit_sig{.flag = noir::consensus::FlagCommit,
    .validator_address = val_addr,
    .timestamp = default_evidence_time,
    .signature = bytes{}});
  return std::make_shared<commit>(commit::new_commit(height, 0, {}, commit_sigs));
}

inline std::vector<noir::consensus::tx> make_txs(int64_t height, int num) {
  std::vector<noir::consensus::tx> txs{};
  for (auto i = 0; i < num; ++i) {
    txs.push_back(noir::consensus::tx{});
  }
  return txs;
}

inline std::shared_ptr<noir::consensus::block> make_block(
  int64_t height, const noir::consensus::state& st, const noir::consensus::commit& commit_) {
  auto txs = make_txs(st.last_block_height, 10);
  auto [block_, part_set_] = const_cast<noir::consensus::state&>(st).make_block(height, txs, commit_, /* {}, */ {});
  // TODO: temparary workaround to set block
  block_->header.height = height;
  return block_;
}

inline std::shared_ptr<block_store> initialize_block_store(const state& state_, const bytes& val_addr) {
  auto block_store_ = std::make_shared<noir::consensus::block_store>(make_session(true, block_store_path));
  auto default_evidence_time = get_default_evidence_time();

  for (auto i = 1; i <= state_.last_block_height; i++) {
    auto last_commit = make_commit(i - 1, val_addr);
    auto block_ = make_block(i, state_, *last_commit);
    block_->header.time =
      default_evidence_time + std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(i)).count();
    block_->header.version = {.block = consensus::block_protocol, .app = 1};
    auto part_set_ = block_->make_part_set(1);
    auto seen_commit = make_commit(i, val_addr);
    block_store_->save_block(*block_, *part_set_, *seen_commit);
  }

  return block_store_;
}

inline std::pair<std::shared_ptr<evidence_pool>, std::shared_ptr<mock_pv>> default_test_pool(int64_t height) {
  auto val = std::make_shared<mock_pv>();
  val->priv_key_ = priv_key::new_priv_key();
  val->pub_key_ = val->priv_key_.get_pub_key();
  auto val_address = val->priv_key_.get_pub_key().address();
  auto evidence_db = make_session(true, evidence_store_path);
  auto state_store_ = initialize_validator_state(val, height);
  state state_;
  state_store_->load(state_);
  auto block_store_ = initialize_block_store(state_, val_address);

  auto pool_ = evidence_pool::new_pool(evidence_db, state_store_, block_store_);
  CHECK(pool_);
  return {pool_.value(), val};
}

} // namespace noir::consensus::ev
