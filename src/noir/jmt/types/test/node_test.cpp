// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/crypto/rand.h>
#include <noir/jmt/types/node.h>

using namespace noir;
using namespace noir::jmt;
using namespace std;

Bytes32 hash_leaf(const Bytes32& key, const Bytes32& value_hash) {
  return sparse_merkle_leaf_node{key, value_hash}.hash();
}

Bytes32 random_Bytes32() {
  Bytes32 out;
  crypto::rand_bytes(out);
  return out;
}

node_key random_63nibbles_node_key() {
  std::vector<uint8_t> bytes(32);
  crypto::rand_bytes(bytes);
  bytes.back() &= 0xf0;
  return {0, nibble_path(std::span(bytes), true)};
}

std::tuple<node_key, Bytes32> gen_leaf_key(version ver, const nibble_path& path, nibble n) {
  check(path.num_nibbles == 63);
  auto np = path;
  np.push(n);
  auto account_key = Bytes32(np.bytes);
  return {node_key{ver, np}, account_key};
}

TEST_CASE("node: encode_decode", "[noir][jmt]") {
  auto internal_node_key = random_63nibbles_node_key();

  auto leaf1_keys = gen_leaf_key(0, internal_node_key.nibble_path, 1);
  auto leaf1_node = node<Bytes>::leaf(std::get<1>(leaf1_keys), Bytes{0x00});
  auto leaf2_keys = gen_leaf_key(0, internal_node_key.nibble_path, 2);
  auto leaf2_node = node<Bytes>::leaf(std::get<1>(leaf2_keys), Bytes{0x01});

  jmt::children children;
  children.insert({1, child{leaf1_node.hash(), 0, leaf{}}});
  children.insert({2, child{leaf2_node.hash(), 0, leaf{}}});

  Bytes32 account_key;
  crypto::rand_bytes(account_key);
  auto nodes = std::vector<node<Bytes>>{
    node<Bytes>::internal(children),
    node<Bytes>::leaf(account_key, Bytes{0x02}),
  };
  for (const auto& n : nodes) {
    auto v = n.encode();
    CHECK(n == node<Bytes>::decode(v));
  }
  {
    auto empty_bytes = std::vector<uint8_t>();
    auto e = node<Bytes>::decode(empty_bytes);
    CHECK(e.error() == "missing tag due to empty input");
  }
  {
    auto unknown_tag_bytes = std::vector<uint8_t>{100};
    auto e = node<Bytes>::decode(unknown_tag_bytes);
    CHECK(e.error() == "lead tag byte is unknown: 100");
  }
}

TEST_CASE("node: internal_validity", "[noir][jmt]") {
  CHECK_THROWS_WITH(internal_node(jmt::children{}), "children must not be empty");
  CHECK_THROWS_WITH(
    []() {
      jmt::children children{};
      children.insert({1, child{random_Bytes32(), 0, leaf{}}});
      internal_node{children};
    }(),
    "if there's only one child, it must not be a leaf");
}

TEST_CASE("node: leaf_hash", "[noir][jmt]") {
  auto address = random_Bytes32();
  auto blob = Bytes{2};
  auto value_hash = default_hasher()(blob);
  auto hash = hash_leaf(address, value_hash);
  auto leaf_node = node<Bytes>::leaf(address, blob);
  CHECK(leaf_node.hash() == hash);
}
