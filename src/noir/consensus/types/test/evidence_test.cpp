// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/bcs.h>
#include <noir/consensus/ev/test/evidence_test_common.h>
#include <noir/consensus/types/evidence.h>
#include <noir/consensus/types/priv_validator.h>
#include <utility>

using namespace noir;
using namespace noir::consensus;
using namespace noir::consensus::ev;

Bytes string_to_bytes(std::string_view s) {
  return {s.begin(), s.end()};
}

std::shared_ptr<duplicate_vote_evidence> random_duplicate_vote_evidence() {
  auto block_id = make_block_id(string_to_bytes("blockhash"), 1000, string_to_bytes("partshash"));
  auto block_id2 = make_block_id(string_to_bytes("blockhash2"), 1000, string_to_bytes("partshash"));
  auto ret = std::make_shared<duplicate_vote_evidence>();
  auto val = mock_pv();
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  std::string chain_id = evidence_chain_id;
  auto default_vote_time = get_default_evidence_time();
  ret->vote_a = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
  ret->vote_b = make_vote(val, chain_id, 0, 10, 2, 1, block_id2,
    default_vote_time + std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::minutes(1)).count());
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
  int64_t height{13};
  auto ev = new_mock_duplicate_vote_evidence(height, get_time(), "mock-chain-id");
  CHECK(ev->get_hash() == crypto::Sha256()(ev->get_bytes()));
  CHECK(ev->get_string() != "");
  CHECK(ev->get_height() == height);
}

TEST_CASE("evidence: duplicate vote validation", "[noir][consensus]") {
  auto val = mock_pv();
  val.priv_key_ = priv_key::new_priv_key();
  val.pub_key_ = val.priv_key_.get_pub_key();
  auto block_id =
    make_block_id(string_to_bytes("blockhash"), std::numeric_limits<int32_t>::max(), string_to_bytes("partshash"));
  auto block_id2 =
    make_block_id(string_to_bytes("blockhash2"), std::numeric_limits<int32_t>::max(), string_to_bytes("partshash"));
  std::string chain_id = evidence_chain_id;
  auto default_vote_time = get_default_evidence_time();

  SECTION("good duplicate") {
    auto vote1 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto vote2 = make_vote(val, chain_id, 0, 10, 2, 1, block_id, default_vote_time);
    auto val_set = validator_set::new_validator_set({{val.pub_key_.address(), {}, 10, 0}});
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
  auto block_id = make_block_id(crypto::Sha256()(string_to_bytes("blockhash")), std::numeric_limits<int32_t>::max(),
    crypto::Sha256()(string_to_bytes("partshash")));
  auto block_id2 = make_block_id(crypto::Sha256()(string_to_bytes("blockhash2")), std::numeric_limits<int32_t>::max(),
    crypto::Sha256()(string_to_bytes("partshash")));
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
  CHECK(ev_list.hash() == from_hex("18dee164df4e56fa81d5c29147e67e0116e342b0ec8872add4d813a0bc329395"));
}

TEST_CASE("evidence: serialization using protobuf", "[noir][consensus]") {
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
    Bytes bz(ev->ByteSizeLong());
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

  SECTION("load duplicate evidence") {
    std::string b =
      "0AC201080110FFFFFFFFFFFFFFFF7F1802224C0A20342594391253BB8F0FBAA422B311D65348071CB7934C402B288B926A034D823B122808"
      "FFFFFFFF07122085818BDC7BAB72042043881846E24BC779E19AC83A3DB92AC5F98721C81209902A060880DBAAE1053214C256F86B30169D"
      "F07BE0AD86718E24FEF0BBF85A38FFFFFFFF074240B20C55CA7584D99C773A0BBBEB7CF1B9F9A1D392D4AA230F3162A441E549778454F7B8"
      "55FE4A9325B8085574E9E89C559FD9CF6B9BEF4FF1515272A7AF54820012C201080110FFFFFFFFFFFFFFFF7F1801224C0A20395BF727F9AA"
      "C5E80911591073FCF9C826F428804131CA089BEBA3869421749A122808FFFFFFFF07122085818BDC7BAB72042043881846E24BC779E19AC8"
      "3A3DB92AC5F98721C81209902A060880DBAAE1053214C256F86B30169DF07BE0AD86718E24FEF0BBF85A38FFFFFFFF0742406DA7E28133E8"
      "33E24249063910D7449E2FFFDEAF6828560AAFCE83861B126CFC114CFE1856DE749A1B37122228947A3ED87A9346938212D268F0C27C4553"
      "3D0E180A20142A060880DBAAE105";
    Bytes bz = from_hex(b);
    ::tendermint::types::DuplicateVoteEvidence pb;
    pb.ParseFromArray(bz.data(), bz.size());
    CHECK(pb.IsInitialized());
    auto ev_dup2 = duplicate_vote_evidence::from_proto(pb).value();
    // std::cout << ev_dup2->get_string() << std::endl;
  }

  SECTION("vote") {
    auto v = *ev_dup->vote_a;
    auto pb = vote::to_proto(v);
    Bytes bz(pb->ByteSizeLong());
    pb->SerializeToArray(bz.data(), pb->ByteSizeLong());
    // std::cout << "pb = " << to_hex(bz) << std::endl;
    // TODO : test vote
  }
}

TEST_CASE("evidence: serialization using datastream", "[noir][consensus]") {
  evidence_list evs;
  auto ev_dup = random_duplicate_vote_evidence();
  evs.list.push_back(ev_dup);

  auto bz_list = encode(evs);
  auto decoded = decode<evidence_list>(bz_list);
  CHECK(evs.list.size() == decoded.list.size());
  CHECK(evs.list[0]->get_string() == decoded.list[0]->get_string());

  auto bz_dup = encode(*ev_dup);
  auto decoded_dup = decode<duplicate_vote_evidence>(bz_dup);
  CHECK(ev_dup->get_hash() == decoded_dup.get_hash());

  block org{block_header{}, block_data{.txs = {{0}, {1}, {2}}}, {}, std::make_unique<commit>()};
  org.evidence.evs = std::make_shared<evidence_list>(evs);
  auto data = encode(org);
  auto decoded_block = decode<block>(data);
  CHECK(org.data.get_hash() == decoded_block.data.get_hash());
}
