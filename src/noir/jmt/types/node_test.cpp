// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/jmt/types/node.h>
#include <openssl/rand.h>

using namespace noir;
using namespace noir::jmt;
using namespace std;

node_key random_63nibbles_node_key() {
  std::vector<uint8_t> bytes(32);
  RAND_bytes(bytes.data(), bytes.size());
  bytes.back() &= 0xf0;
  return {0, nibble_path(bytes, true)};
}

std::tuple<node_key, bytes32> gen_leaf_key(version ver, const nibble_path& path, nibble n) {
  check(path.num_nibbles == 63);
  auto np = path;
  np.push(n);
  auto account_key = bytes32((char*)np.bytes.data(), np.bytes.size());
  return {node_key{ver, np}, account_key};
}

TEST_CASE("[node] encode/decode", "[jmt]") {
  auto internal_node_key = random_63nibbles_node_key();

  auto leaf1_keys = gen_leaf_key(0, internal_node_key.nibble_path, 1);
  auto leaf1_node = node<bytes>::leaf(std::get<1>(leaf1_keys), bytes{0x00});
  auto leaf2_keys = gen_leaf_key(0, internal_node_key.nibble_path, 2);
  auto leaf2_node = node<bytes>::leaf(std::get<1>(leaf2_keys), bytes{0x01});

  auto children = jmt::children();
  children.insert(std::make_pair(nibble(1), child{leaf1_node.hash(), 0, leaf{}}));
  children.insert(std::make_pair(nibble(2), child{leaf2_node.hash(), 0, leaf{}}));

  bytes32 account_key;
  RAND_bytes((uint8_t*)account_key.data(), account_key.size());
  auto nodes = std::vector<node<bytes>>{
    node<bytes>::internal(children),
    node<bytes>::leaf(account_key, bytes{0x02}),
  };
  for (const auto& n : nodes) {
    auto v = n.encode();
    CHECK(n == node<bytes>::decode(v));
  }
  /*
  auto empty_bytes = std::vector<uint8_t>();
  auto e = node<bytes>::decode(empty_bytes);
  */
}
