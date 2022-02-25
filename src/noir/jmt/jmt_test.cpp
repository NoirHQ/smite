// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/jmt/jmt.h>
#include <noir/jmt/mock_tree_store.h>
#include <openssl/rand.h>

using namespace noir;
using namespace noir::jmt;

auto random_bytes32() {
  bytes32 out;
  RAND_bytes((uint8_t*)out.data(), out.size());
  return out;
}

auto update_nibble(bytes32& original_key, size_t n, uint8_t nibble) {
  check(nibble < 16);
  auto key = original_key;
  key[n / 2] = !(n % 2) ? ((key[n / 2] & 0xf) | nibble << 4) : ((key[n / 2] & 0xf0) | nibble);
  return key;
}

using value_blob = std::vector<char>;

TEST_CASE("[jmt] insert_to_empty_tree", "[jmt]") {
  auto db = mock_tree_store<value_blob>();
  auto tree = jellyfish_merkle_tree(db);

  auto key = random_bytes32();
  auto value = value_blob{1, 2, 3, 4};

  auto [_new_root_hash, batch] = *tree.batch_put_value_sets({{{key, value}}}, {}, 0);
  CHECK(batch.stale_node_index_batch.empty());

  db.write_tree_update_batch(batch);
  CHECK(**tree.get(key, 0) == value);
}

TEST_CASE("[jmt] insert_to_pre_genesis", "[jmt]") {
  auto db = mock_tree_store<value_blob>();
  auto key1 = bytes32();
  auto value1 = value_blob{1, 2};
  auto pre_genesis_root_key = node_key{pre_genesis_version};
  db.put_node(pre_genesis_root_key, node<value_blob>::leaf(key1, value1));

  auto tree = jellyfish_merkle_tree(db);
  auto key2 = update_nibble(key1, 0, 15);
  auto value2 = value_blob{3, 4};

  auto [_root_hash, batch] = *tree.batch_put_value_sets({{{key2, value2}}}, {}, 0);

  CHECK(batch.stale_node_index_batch.size() == 1);
  db.write_tree_update_batch(batch);
  CHECK(db.num_nodes() == 4);
  db.purge_stale_nodes(0);
  CHECK(db.num_nodes() == 3);

  CHECK(**tree.get(key1, 0) == value1);
  CHECK(**tree.get(key2, 0) == value2);
}

TEST_CASE("[jmt] insert_at_leaf_with_internal_created", "[jmt]") {
  auto db = mock_tree_store<value_blob>();
  auto tree = jellyfish_merkle_tree(db);

  auto key1 = bytes32();
  auto value1 = value_blob{1, 2};

  auto [_root0_hash, batch0] = *tree.batch_put_value_sets({{{key1, value1}}}, {}, 0);
  CHECK(batch0.stale_node_index_batch.empty());
  db.write_tree_update_batch(batch0);
  CHECK(**tree.get(key1, 0) == value1);

  auto key2 = update_nibble(key1, 0, 15);
  auto value2 = value_blob{3, 4};

  auto [_root1_hash, batch1] = *tree.batch_put_value_sets({{{key2, value2}}}, {}, 1);
  CHECK(batch1.stale_node_index_batch.size() == 1);
  db.write_tree_update_batch(batch1);

  CHECK(**tree.get(key1, 0) == value1);
  CHECK(tree.get(key2, 0)->has_value() == false);
  CHECK(**tree.get(key2, 1) == value2);

  CHECK(db.num_nodes() == 4);

  auto internal_node_key = node_key{1};

  auto leaf1 = node<value_blob>::leaf(key1, value1);
  auto leaf2 = node<value_blob>::leaf(key2, value2);
  jmt::children children;
  children.insert({0, child{leaf1.hash(), 1, leaf{}}});
  children.insert({15, child{leaf2.hash(), 1, leaf{}}});
  auto internal = node<value_blob>::internal(children);
  CHECK(db.get_node(node_key{0}) == leaf1);
  CHECK(db.get_node(internal_node_key.gen_child_node_key(1, 0)) == leaf1);
  CHECK(db.get_node(internal_node_key.gen_child_node_key(1, 15)) == leaf2);
  CHECK(db.get_node(internal_node_key) == internal);
}
