// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>
#include <noir/consensus/evidence/evidence_pool.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types/priv_validator.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::consensus::ev;

constexpr auto evidence_chain_id = "my_chain";
constexpr auto evidence_store_path = "/tmp/ev";
constexpr auto state_store_path = "/tmp/ev_state";
constexpr auto block_store_path = "/tmp/ev_block";
constexpr int64_t default_evidence_max_bytes = 1000;

tstamp get_default_evidence_time() {
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

p2p::block_id make_block_id(bytes hash, uint32_t part_set_size, bytes part_set_hash) {
  return {.hash = std::move(hash), .parts = {.total = part_set_size, .hash = std::move(part_set_hash)}};
}

std::shared_ptr<vote> make_vote(mock_pv& val,
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

std::shared_ptr<duplicate_vote_evidence> mock_duplicate_vote_evidence() {
  auto block_id = make_block_id(from_hex("0000"), 1000, from_hex("AAAA"));
  auto block_id2 = make_block_id(from_hex("0001"), 1000, from_hex("AAAA"));
  auto ret = std::make_shared<duplicate_vote_evidence>();
  auto val = mock_pv();
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  std::string chain_id = evidence_chain_id;
  auto default_vote_time = get_default_evidence_time();
  ret->vote_a = make_vote(val, chain_id, 0, 1, 0, 2, block_id, default_vote_time);
  ret->vote_b = make_vote(val, chain_id, 0, 1, 0, 2, block_id2, default_vote_time);
  ret->total_voting_power = 30;
  ret->validator_power = 10;
  ret->timestamp = default_vote_time;
  return ret;
}

std::shared_ptr<noir::consensus::db_store> initialize_state_from_validator_set(
  std::shared_ptr<validator_set> val_set, int64_t height) {
  auto default_evidence_time = get_default_evidence_time();
  auto state_store_ = std::make_shared<noir::consensus::db_store>(make_session(true, state_store_path));
  auto state_ = state{.chain_id = evidence_chain_id,
    .initial_height = 1,
    .last_block_height = height,
    .last_block_time = default_evidence_time,
    .validators = *val_set,
    .next_validators = val_set->copy_increment_proposer_priority(1),
    .last_validators = *val_set,
    .last_height_validators_changed = 1,
    .consensus_params_ = consensus_params{.block = {.max_bytes = 22020096, .max_gas = -1},
      .evidence = {.max_age_num_blocks = 20, .max_age_duration = 20, .max_bytes = 1000}}};

  for (auto i = 0; i <= height; i++) {
    state_.last_block_height = i;
    state_store_->save(state_);
  }

  return state_store_;
}

std::shared_ptr<noir::consensus::db_store> initialize_validator_state(
  std::shared_ptr<mock_pv> priv_val, int64_t height) {
  auto pub_key_ = priv_val->get_pub_key();
  auto val = validator::new_validator(pub_key_, 10);
  auto val_set = std::make_shared<validator_set>();
  val_set->validators.push_back(val);
  val_set->proposer = val;
  return initialize_state_from_validator_set(val_set, height);
}

std::shared_ptr<commit> make_commit(int64_t height, bytes val_addr) {
  auto default_evidence_time = get_default_evidence_time();
  auto commit_sigs = std::vector<commit_sig>();
  commit_sigs.push_back(commit_sig{.flag = noir::consensus::FlagCommit,
    .validator_address = val_addr,
    .timestamp = default_evidence_time,
    .signature = bytes{}});
  return std::make_shared<commit>(commit::new_commit(height, 0, {}, commit_sigs));
}

std::shared_ptr<block_store> initialize_block_store(state state_, bytes val_addr) {
  auto block_store_ = std::make_shared<noir::consensus::block_store>(make_session(true, block_store_path));
  auto default_evidence_time = get_default_evidence_time();

  for (auto i = 1; i <= state_.last_block_height; i++) {
    auto last_commit = make_commit(i - 1, val_addr);
    auto block_ = make_block(i, state_, *last_commit);
    block_->header.time = default_evidence_time + i;
    block_->header.version = ""; // TODO: use correct version
    auto part_set_ = block_->make_part_set(1);
    auto seen_commit = make_commit(i, val_addr);
    block_store_->save_block(*block_, *part_set_, *seen_commit);
  }

  return block_store_;
}

std::pair<std::shared_ptr<evidence_pool>, std::shared_ptr<mock_pv>> default_test_pool(int64_t height) {
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

TEST_CASE("evidence_pool: verify pending evidence passes", "[noir][consensus]") {
  int64_t height{1};
  auto [pool_, val] = default_test_pool(height);
  auto ev = mock_duplicate_vote_evidence();
  // auto add_result = pool_->add_evidence(ev); // FIXME
  // CHECK(add_result);
  // std::cout << add_result.error() << std::endl;
  // evidence_list evs{.list = {ev}};
  // auto check_result = pool_->check_evidence(evs);
}

TEST_CASE("evidence_pool: basic", "[noir][consensus]") {
  int64_t height{1};
  auto [pool_, val] = default_test_pool(height);

  auto [evs, size] = pool_->pending_evidence(default_evidence_max_bytes);
  CHECK(evs.empty());

  auto ev = mock_duplicate_vote_evidence();
  // pool_->add_evidence(ev);
  // CHECK(evs.size() == 1);
}
