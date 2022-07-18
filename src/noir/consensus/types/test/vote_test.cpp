// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/protobuf.h>
#include <noir/common/hex.h>
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/priv_validator.h>
#include <noir/consensus/types/vote.h>

#include <cppcodec/base64_default_rfc4648.hpp>
#include <date/date.h>

using namespace noir;
using namespace noir::consensus;

Bytes string_to_bytes(std::string_view s) {
  return {s.begin(), s.end()};
}

vote example_vote(p2p::signed_msg_type type_) {
  using namespace date;
  using namespace std::chrono;
  date::sys_time<microseconds> tp = date::sys_days{date::December / 25 / 2017} + 3h + 1s + 234ms;
  auto stamp = tp.time_since_epoch().count();

  auto sha256 = crypto::Sha256();
  auto hash_address = sha256(string_to_bytes("validator_address"));

  vote vote_;
  vote_.type = type_;
  vote_.height = 12345;
  vote_.round = 2;
  vote_.timestamp = stamp;
  vote_.block_id_ = {.hash = sha256(string_to_bytes("blockID_hash")),
    .parts = {
      .total = 1000000,
      .hash = sha256(string_to_bytes("blockID_part_set_header_hash")),
    }};
  vote_.validator_address = {hash_address.begin(), hash_address.begin() + 20};
  vote_.validator_index = 56789;
  return vote_;
}

vote example_prevote() {
  return example_vote(noir::p2p::Prevote);
}
vote example_precommit() {
  return example_vote(noir::p2p::Precommit);
}

TEST_CASE("vote: verify signature", "[noir][consensus]") {
  auto priv_key_str =
    base64::decode("Cx7Y3j64wh94YB2OFO/14Xd+hZnHVDZJC1GeK+PQU7V57alcK/VkCFlZopomniat7dieJ0n+o4CVaFluB9tdHw==");
  auto val = mock_pv();
  val.priv_key_.key = Bytes{priv_key_str.begin(), priv_key_str.end()};
  val.pub_key_ = val.priv_key_.get_pub_key();
  CHECK(val.pub_key_.address() == Bytes("6A8C99080264E020EBB52292AA9F6E4293BB71E1"));

  auto vote_ = example_precommit();
  auto pb_vote_ = vote::to_proto(vote_);

  auto pb = canonical::canonicalize_vote_pb("test_chain_id", *pb_vote_);

  auto bz_plain = codec::protobuf::encode(pb);
  CHECK(bz_plain.size() == 124);

  auto bz_sign_bytes = vote::vote_sign_bytes("test_chain_id", *pb_vote_);
  CHECK(bz_sign_bytes.size() == 125);

  // Sign
  CHECK(!val.sign_vote("test_chain_id", vote_).has_error());

  // Verify
  CHECK(val.get_pub_key().verify_signature(bz_sign_bytes, vote_.signature));
}
