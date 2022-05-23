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

bytes string_to_bytes(std::string_view s) {
  return {s.begin(), s.end()};
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
  val.priv_key_.key = from_hex("9D834D3FCAC4EE25536B70E15376FEBB697861AB573405D8B89290417FD16070556A436F1218D30942EFE79"
                               "8420F51DC9B6A311B929C578257457D05C5FCF230");
  val.pub_key_ = val.priv_key_.get_pub_key();
  auto block_id = make_block_id(crypto::sha256()(string_to_bytes("blockhash")), std::numeric_limits<int32_t>::max(),
    crypto::sha256()(string_to_bytes("partshash")));
  auto block_id2 = make_block_id(crypto::sha256()(string_to_bytes("blockhash2")), std::numeric_limits<int32_t>::max(),
    crypto::sha256()(string_to_bytes("partshash")));
  std::string chain_id = "mychain";
  auto v = make_vote(
    val, chain_id, std::numeric_limits<int32_t>::max(), std::numeric_limits<int64_t>::max(), 1, 1, block_id, 1000000);
  auto v2 = make_vote(
    val, chain_id, std::numeric_limits<int32_t>::max(), std::numeric_limits<int64_t>::max(), 2, 1, block_id2, 1000000);

  // Data for light_client_attack_evidence
  int64_t height{5};
  auto common_height = height - 1;
  int n_validators = 10;
  // TODO: requires many more helper functions to continue

  auto dup = std::make_shared<duplicate_vote_evidence>();
  dup->vote_a = v2;
  dup->vote_b = v;
  evidence_list ev_list{{dup}};
  // std::cout << to_hex(ev_list.hash()) << std::endl;
  CHECK(ev_list.hash() == from_hex("b34b4bb0b31dba1c38185ea0e09b70fa8cd451431dfc3d15f4a234c3e9c0bd23"));
}

TEST_CASE("evidence: serialization", "[noir][consensus]") {
  auto ev_dup = random_duplicate_vote_evidence();

  SECTION("duplicate evidence") {
    auto bz = ev_dup->get_bytes();
    // std::cout << to_hex(bz) << std::endl;
    // std::cout << ev_dup->get_string() << std::endl;

    ::tendermint::types::DuplicateVoteEvidence pb;
    pb.ParseFromArray(bz.data(), bz.size());
    CHECK(pb.IsInitialized());
    auto ev_dup2 = duplicate_vote_evidence::from_proto(pb).value();
    // std::cout << ev_dup2->get_string() << std::endl;
    CHECK(ev_dup->vote_a->signature == ev_dup2->vote_a->signature);
    CHECK(ev_dup->vote_b->signature == ev_dup2->vote_b->signature);
    CHECK(ev_dup->total_voting_power == ev_dup2->total_voting_power);
    CHECK(ev_dup->validator_power == ev_dup2->validator_power);
    CHECK(ev_dup->timestamp == ev_dup2->timestamp);
  }

  SECTION("evidence") {
    auto ev = evidence::to_proto(*ev_dup).value();
    bytes bz(ev->ByteSizeLong());
    ev->SerializeToArray(bz.data(), ev->ByteSizeLong());
    // std::cout << to_hex(bz) << std::endl;

    ::tendermint::types::Evidence pb;
    pb.ParseFromArray(bz.data(), bz.size());
    CHECK(pb.IsInitialized());
    auto ev2 = evidence::from_proto(pb).value();
    // std::cout << ev2->get_string() << std::endl;
    CHECK(ev_dup->get_string() == ev2->get_string());
    CHECK(ev_dup->get_hash() == ev2->get_hash());
  }
}
