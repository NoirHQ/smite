// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <catch2/catch_all.hpp>
#include <noir/consensus/ev/test/evidence_test_common.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::consensus::ev;

constexpr int64_t default_evidence_max_bytes = 1000;

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
