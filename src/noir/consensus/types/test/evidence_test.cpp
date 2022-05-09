// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/priv_validator.h>
#include <noir/consensus/types/evidence.h>

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
  auto block_id = make_block_id(from_hex("blockhash"), 1000, from_hex("partshash"));
  auto block_id2 = make_block_id(from_hex("blockhash2"), 1000, from_hex("partshash"));
  std::string chain_id = "my_chain";
  auto default_vote_time = get_time();
  auto ret = std::make_shared<duplicate_vote_evidence>();
  auto val = mock_pv();
  val.pub_key_.key = from_hex("pub");
  val.priv_key_.key = from_hex("priv");
  ret->vote_a = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
  ret->vote_b = make_vote(val, chain_id, 0, 10, 2, 1, block_id2, default_vote_time + 1);
  ret->total_voting_power = 30;
  ret->validator_power = 10;
  ret->timestamp = default_vote_time;
  return ret;
}

TEST_CASE("evidence: load or gen", "[noir][consensus]") {}
