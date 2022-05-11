// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/priv_validator.h>
#include <utility>

using namespace noir;
using namespace noir::consensus;

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

std::shared_ptr<duplicate_vote_evidence> random_duplicate_vote_evidence() {
  auto block_id = make_block_id(from_hex("0000"), 1000, from_hex("AAAA"));
  auto block_id2 = make_block_id(from_hex("0001"), 1000, from_hex("AAAA"));
  auto ret = std::make_shared<duplicate_vote_evidence>();
  auto val = mock_pv();
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  std::string chain_id = "my_chain";
  auto default_vote_time = get_time();
  ret->vote_a = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
  ret->vote_b = make_vote(val, chain_id, 0, 10, 2, 1, block_id2, default_vote_time + 1);
  ret->total_voting_power = 30;
  ret->validator_power = 10;
  ret->timestamp = default_vote_time;
  return ret;
}

TEST_CASE("evidence: list", "[noir][consensus]") {
  auto ev = random_duplicate_vote_evidence();
  evidence_list evl{{ev}};
  CHECK(!evl.hash().empty());
  CHECK(evl.has(ev));
  CHECK(!evl.has(std::make_shared<duplicate_vote_evidence>()));
}

TEST_CASE("evidence: duplicate vote", "[noir][consensus]") {
  auto val = mock_pv();
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  auto block_id = make_block_id(from_hex("0000"), 1000, from_hex("AAAA"));
  auto block_id2 = make_block_id(from_hex("0001"), 1000, from_hex("AAAA"));
  std::string chain_id = "my_chain";
  auto default_vote_time = get_time();

  SECTION("good duplicate") {
    auto vote1 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto vote2 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto val_set =
      std::make_shared<validator_set>(validator_set::new_validator_set({{val.pub_key_.address(), {}, 10, 0}}));
    auto ev = duplicate_vote_evidence::new_duplicate_vote_evidence(vote1, vote2, default_vote_time, val_set);
    CHECK(ev);
    CHECK(!ev.value()->validate_basic());
  }
}

TEST_CASE("evidence: verify generated hash", "[noir][consensus]") {
  auto val = mock_pv();

  // TODO: requires ed25519 to finish; come back later to implement after ed25519 is available
  /// duplicateVoteEvidence = a9ce28d13bb31001fc3e5b7927051baf98f86abdbd64377643a304164c826923
  /// LightClientAttackEvidence = 2f8782163c3905b26e65823ababc977fe54e97b94e60c0360b1e4726b668bb8e
}
