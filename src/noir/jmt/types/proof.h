// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/bytes.h>
#include <noir/crypto/hash/sha3.h>
#include <noir/jmt/types/common.h>
#include <optional>

namespace noir::jmt {

const char noir_hash_prefix[] = "DIEM::";
const char sparse_merkle_internal_salt[] = "SparseMerkleInternal";

using default_hasher = crypto::sha3_256;

template<const char* SEED>
struct merkle_tree_internal_node {
  bytes32 hash() {
    default_hasher hasher;
    auto buffer = std::string(noir_hash_prefix) + SEED;
    auto seed = hasher(buffer);
    hasher.init(); // reset hasher context
    hasher.update(seed);
    hasher.update({left_child.data(), left_child.size()});
    hasher.update({right_child.data(), right_child.size()});
    auto bytes = hasher.final();
    return bytes32{bytes};
  }

  bytes32 left_child;
  bytes32 right_child;
};

using sparse_merkle_internal_node = merkle_tree_internal_node<sparse_merkle_internal_salt>;

struct sparse_merkle_leaf_node {
  sparse_merkle_leaf_node(bytes32 key, bytes32 value_hash): key(key), value_hash(value_hash) {}

  bytes32 hash() {
    default_hasher hasher;
    auto buffer = std::string(noir_hash_prefix) + "SparseMerkleLeafNode";
    auto seed = hasher(buffer);
    hasher.init(); // reset hasher context
    hasher.update(seed);
    hasher.update({key.data(), key.size()});
    hasher.update({value_hash.data(), value_hash.size()});
    auto bytes = hasher.final();
    return bytes32{bytes};
  }

  bytes32 key;
  bytes32 value_hash;
};

template<typename T>
struct sparse_merkle_proof {
  auto verify(const bytes32& expected_root_hash, const bytes32& element_key,
    std::optional<std::remove_reference_t<T>> element_value) -> result<void> {
    ensure(siblings.size() <= 256, "sparse merkle tree proof has more than 256 ({}) siblings", siblings.size());
    if (element_value) {
      auto& value = element_value->get();
      if (leaf) {
        ensure(element_key == leaf->key, "keys do not match. key in proof: {}, expected: {}", leaf->key.to_string(),
          element_key.to_string());
        auto hash = value.hash();
        ensure(hash == leaf->value_hash, "value hashes do not match. value hash in proof: {}, expected: {}",
          leaf->value_hash, hash);
      } else {
        bail("expected inclusion proof. found non-inclusion proof");
      }
    } else {
      if (leaf) {
        ensure(element_key != leaf->key, "expected non-inclusion proof, but key exists in proof");
        ensure(common_prefix_bits_len(element_key, leaf->key) >= siblings.size(),
          "key would not have ended up in the subtree where the provided key in proof"
          "is the only existing key, if it existed. so this is not a valid"
          "non-includsion proof");
      }
    }
    auto current_hash = (leaf) ? leaf->hash() : sparse_merkle_placeholder_hash;
    auto actual_root_hash = bytes32();
    ensure(actual_root_hash == expected_root_hash, "root hashes do not match. actual root hash: {}, expected: {}",
      actual_root_hash.to_string(), expected_root_hash.to_string());
    return {};
  }

  std::optional<sparse_merkle_leaf_node> leaf;
  std::vector<bytes32> siblings;
};

} // namespace noir::jmt
