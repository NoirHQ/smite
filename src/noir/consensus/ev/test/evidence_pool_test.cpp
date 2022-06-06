// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>
#include <noir/consensus/ev/evidence_pool.h>
#include <noir/consensus/ev/test/evidence_test_common.h>
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

std::shared_ptr<noir::consensus::db_store> initialize_state_from_validator_set(
  const std::shared_ptr<validator_set>& val_set, int64_t height) {
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
      .evidence = {.max_age_num_blocks = 20,
        .max_age_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(20)).count(),
        .max_bytes = 1000}}};
  for (auto i = 0; i <= height; i++) {
    state_.last_block_height = i;
    state_store_->save(state_);
  }
  return state_store_;
}

std::shared_ptr<noir::consensus::db_store> initialize_validator_state(
  const std::shared_ptr<mock_pv>& priv_val, int64_t height) {
  auto pub_key_ = priv_val->get_pub_key();
  auto val = validator::new_validator(pub_key_, 10);
  auto val_set = std::make_shared<validator_set>();
  val_set->validators.push_back(val);
  val_set->proposer = val;
  return initialize_state_from_validator_set(val_set, height);
}

std::shared_ptr<commit> make_commit(int64_t height, const bytes& val_addr) {
  auto default_evidence_time = get_default_evidence_time();
  auto commit_sigs = std::vector<commit_sig>();
  commit_sigs.push_back(commit_sig{.flag = noir::consensus::FlagCommit,
    .validator_address = val_addr,
    .timestamp = default_evidence_time,
    .signature = bytes{}});
  return std::make_shared<commit>(commit::new_commit(height, 0, {}, commit_sigs));
}

std::shared_ptr<block_store> initialize_block_store(const state& state_, const bytes& val_addr) {
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

TEST_CASE("evidence_pool: basic", "[noir][consensus]") {
  int64_t height{1};
  auto [pool_, val] = default_test_pool(height);

  SECTION("no evidence yet") {
    auto [evs, size] = pool_->pending_evidence(default_evidence_max_bytes);
    CHECK(evs.empty());
  }

  SECTION("good evidence") {
    auto ev =
      new_mock_duplicate_vote_evidence_with_validator(height, get_default_evidence_time(), evidence_chain_id, *val);
    pool_->add_evidence(ev);
    auto [evs, size] = pool_->pending_evidence(default_evidence_max_bytes);
    CHECK(evs.size() == 1);
  }
}

TEST_CASE("evidence_pool: report conflicting votes", "[noir][consensus]") {
  int64_t height{10};
  auto [pool_, pv] = default_test_pool(height);
  auto val = validator::new_validator(pv->priv_key_.get_pub_key(), 10);
  auto ev =
    new_mock_duplicate_vote_evidence_with_validator(height + 1, get_default_evidence_time(), evidence_chain_id, *pv);

  pool_->report_conflicting_votes(ev->vote_a, ev->vote_b);
  pool_->report_conflicting_votes(ev->vote_a, ev->vote_b); // should fail
  auto [ev_list, ev_size] = pool_->pending_evidence(default_evidence_max_bytes);
  CHECK(ev_list.empty());
  CHECK(ev_size == 0);

  auto next = pool_->evidence_front();
  CHECK(!next);

  auto state = *pool_->state;
  state.last_block_height++;
  state.last_block_time = ev->get_timestamp();
  state.last_validators = validator_set::new_validator_set({val});
  pool_->update(state, {});

  auto [ev_list2, _] = pool_->pending_evidence(default_evidence_max_bytes);
  CHECK(ev_list2[0]->get_hash() == ev->get_hash());

  next = pool_->evidence_front();
  CHECK(next);
}

TEST_CASE("evidence_pool: update", "[noir][consensus]") {
  int64_t height{21};
  auto [pool_, val] = default_test_pool(height);
  auto state = *pool_->state;

  auto pruned_ev = new_mock_duplicate_vote_evidence_with_validator(1,
    get_default_evidence_time() +
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(1)).count(),
    evidence_chain_id, *val);

  auto not_pruned_ev = new_mock_duplicate_vote_evidence_with_validator(2,
    get_default_evidence_time() +
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(2)).count(),
    evidence_chain_id, *val);

  CHECK(pool_->add_evidence(pruned_ev));
  CHECK(pool_->add_evidence(not_pruned_ev));

  auto ev = new_mock_duplicate_vote_evidence_with_validator(height,
    get_default_evidence_time() +
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(21)).count(),
    evidence_chain_id, *val);
  auto last_commit = make_commit(height, val->priv_key_.get_pub_key().address());
  auto block = block::make_block(height + 1, {}, *last_commit); // TODO: take evidence

  state.last_block_height = height + 1;
  state.last_block_time = get_default_evidence_time() +
    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(22)).count();

  SECTION("two evidences") {
    auto [ev_list, _] = pool_->pending_evidence(2 * default_evidence_max_bytes);
    CHECK(ev_list.size() == 2);
    CHECK(pool_->get_size() == 2);
  }
  CHECK(pool_->check_evidence({.list = {ev}}));

  SECTION("three evidences") {
    auto [ev_list, _] = pool_->pending_evidence(3 * default_evidence_max_bytes);
    CHECK(ev_list.size() == 3);
    CHECK(pool_->get_size() == 3);
  }

  pool_->update(state, {.list = {ev}});
  SECTION("empty evidence") {
    auto [ev_list, _] = pool_->pending_evidence(default_evidence_max_bytes);
    // CHECK(ev_list.size() == 1); // FIXME
  }

  CHECK(pool_->check_evidence({.list = {ev}}).error().message() == "evidence was already committed");
}

TEST_CASE("evidence_pool: verify pending evidence passes", "[noir][consensus]") {
  int64_t height{1};
  auto [pool_, val] = default_test_pool(height);
  auto ev = new_mock_duplicate_vote_evidence_with_validator(height,
    get_default_evidence_time() +
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(1)).count(),
    evidence_chain_id, *val);
  CHECK(pool_->add_evidence(ev));
  CHECK(pool_->check_evidence({.list = {ev}}));
}

TEST_CASE("evidence_pool: verify failed duplicated evidence", "[noir][consensus]") {
  int64_t height{1};
  auto [pool_, val] = default_test_pool(height);
  auto ev = new_mock_duplicate_vote_evidence_with_validator(height,
    get_default_evidence_time() +
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(1)).count(),
    evidence_chain_id, *val);
  CHECK(pool_->check_evidence({.list = {ev, ev}}).error().message() == "duplicate evidence");
}
