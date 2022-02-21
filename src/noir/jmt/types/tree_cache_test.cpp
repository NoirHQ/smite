// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/types/bytes.h>
#include <noir/jmt/mock_tree_store.h>
#include <noir/jmt/types/node.h>
#include <noir/jmt/types/tree_cache.h>
#include <openssl/rand.h>

using namespace noir;
using namespace noir::jmt;

auto random_bytes32() {
  bytes32 out;
  RAND_bytes((uint8_t*)out.data(), out.size());
  return out;
}

using node_ = jmt::node<std::vector<char>>;

auto random_leaf_with_key(version next_version) -> std::pair<node_, node_key> {
  auto address = random_bytes32();
  std::vector<char> blob(32);
  RAND_bytes((uint8_t*)blob.data(), blob.size());
  auto node = node_::leaf(address, blob);
  auto node_key = jmt::node_key{next_version, nibble_path({(uint8_t*)address.data(), address.size()})};
  return {node, node_key};
}

using reader_type = mock_tree_store<std::vector<char>>;

TEST_CASE("[tree_cache] get_node", "[jmt]") {
  auto next_version = 0;
  auto db = reader_type();
  auto cache = tree_cache<reader_type, std::vector<char>>(db, next_version);

  // auto [node, node_key] = random_leaf_with_key(next_version);
  // db.put_node(node_key, node);

  // CHECK(cache.get_node(node_key).value() == node);
}
