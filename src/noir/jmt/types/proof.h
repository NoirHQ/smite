// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/crypto/hash/sha3.h>
#include <noir/jmt/types/common.h>
#include <optional>

namespace noir::jmt {

const char noir_hash_prefix[] = "NOIR::";
const char sparse_merkle_internal_salt[] = "SparseMerkleInternal";

using default_hasher = crypto::Sha3_256;

template<const char* SEED>
struct merkle_tree_internal_node {
  Bytes32 hash() {
    default_hasher hasher;
    auto buffer = std::string(noir_hash_prefix) + SEED;
    auto seed = hasher(buffer);
    hasher.init(); // reset hasher context
    hasher.update(seed);
    hasher.update(left_child);
    hasher.update(right_child);
    return Bytes32{hasher.final()};
  }

  Bytes32 left_child;
  Bytes32 right_child;
};

using sparse_merkle_internal_node = merkle_tree_internal_node<sparse_merkle_internal_salt>;

struct sparse_merkle_leaf_node {
  sparse_merkle_leaf_node(Bytes32 key, Bytes32 value_hash): key(key), value_hash(value_hash) {}

  Bytes32 hash() {
    default_hasher hasher;
    auto buffer = std::string(noir_hash_prefix) + "SparseMerkleLeafNode";
    auto seed = hasher(buffer);
    hasher.init(); // reset hasher context
    hasher.update(seed);
    hasher.update(key);
    hasher.update(value_hash);
    return Bytes32{hasher.final()};
  }

  Bytes32 key;
  Bytes32 value_hash;
};

template<typename T>
struct sparse_merkle_proof {
  auto verify(const Bytes32& expected_root_hash,
    const Bytes32& element_key,
    std::optional<std::remove_reference_t<T>> element_value) -> result<void> {
    noir_ensure(siblings.size() <= 256, "sparse merkle tree proof has more than 256 ({}) siblings", siblings.size());
    if (element_value) {
      auto& value = *element_value;
      if (leaf) {
        noir_ensure(element_key == leaf->key, "keys do not match. key in proof: {}, expected: {}",
          leaf->key.to_string(), element_key.to_string());
        Bytes32 hash;
        default_hasher{}(value, hash);
        noir_ensure(hash == leaf->value_hash, "value hashes do not match. value hash in proof: {}, expected: {}",
          leaf->value_hash.to_string(), hash.to_string());
      } else {
        noir_bail("expected inclusion proof. found non-inclusion proof");
      }
    } else {
      if (leaf) {
        noir_ensure(element_key != leaf->key, "expected non-inclusion proof, but key exists in proof");
        noir_ensure(common_prefix_bits_len(element_key, leaf->key) >= siblings.size(),
          "key would not have ended up in the subtree where the provided key in proof "
          "is the only existing key, if it existed. so this is not a valid "
          "non-includsion proof");
      }
    }
    auto current_hash = (leaf) ? leaf->hash() : sparse_merkle_placeholder_hash;
    auto actual_root_hash = [&]() {
      auto hash = current_hash;
      auto element_key_bits = element_key.to_bitset();
      auto element_key_bits_index = (256 - siblings.size());
      for (auto it = siblings.begin(); it != siblings.end(); ++it, ++element_key_bits_index) {
        sparse_merkle_internal_node node;
        if (element_key_bits.test(element_key_bits_index)) {
          node.left_child = *it;
          node.right_child = hash;
        } else {
          node.left_child = hash;
          node.right_child = *it;
        }
        hash = node.hash();
      }
      return hash;
    }();
    noir_ensure(actual_root_hash == expected_root_hash, "root hashes do not match. actual root hash: {}, expected: {}",
      actual_root_hash.to_string(), expected_root_hash.to_string());
    return {};
  }

  std::optional<sparse_merkle_leaf_node> leaf;
  std::vector<Bytes32> siblings;
};

} // namespace noir::jmt
