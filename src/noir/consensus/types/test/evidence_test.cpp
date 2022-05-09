// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/priv_validator.h>

using namespace noir;
using namespace noir::consensus;

p2p::block_id make_block_id(bytes hash, uint32_t part_set_size, bytes part_set_hash) {
  return {.hash = hash, .parts = {.total = part_set_size, .hash = part_set_hash}};
}

std::shared_ptr<vote> make_vote(mock_pv& val,
  const std::string& chain_id,
  int32_t val_index,
  int64_t height,
  int32_t round,
  int step,
  const p2p::block_id& block_id_,
  p2p::tstamp time) {
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
  val.pub_key_.key = from_hex("02c91a61a1ad0cf9deb1a394cd97972968503f13d67d75d05fa4b022ee402d713b2e8768ae");
  val.priv_key_.key = from_hex("803bc1dc3ae9028611ec2aa0a47b7646a7d4b962093239c46ecd40c22a0597e67070444e33");
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
  val.pub_key_.key = from_hex("02c91a61a1ad0cf9deb1a394cd97972968503f13d67d75d05fa4b022ee402d713b2e8768ae");
  val.priv_key_.key = from_hex("803bc1dc3ae9028611ec2aa0a47b7646a7d4b962093239c46ecd40c22a0597e67070444e33");
  auto block_id = make_block_id(from_hex("0000"), 1000, from_hex("AAAA"));
  auto block_id2 = make_block_id(from_hex("0001"), 1000, from_hex("AAAA"));
  std::string chain_id = "my_chain";
  auto default_vote_time = get_time();

  SECTION("good duplicate") {
    auto vote1 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto vote2 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto val_set = std::make_shared<validator_set>(validator_set::new_validator_set(
      {{from_hex("02c91a61a1ad0cf9deb1a394cd97972968503f13d67d75d05fa4b022ee402d713b2e8768ae"), {}, 10, 0}}));
    auto ev = duplicate_vote_evidence::new_duplicate_vote_evidence(vote1, vote2, default_vote_time, val_set);
    CHECK(ev);
    CHECK(!ev.value()->validate_basic());
  }
}
