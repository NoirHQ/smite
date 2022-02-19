// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/bytes.h>
#include <noir/crypto/hash/sha3.h>

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
    return bytes32{bytes.data(), bytes.size()};
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
    return bytes32{bytes.data(), bytes.size()};
  }

  bytes32 key;
  bytes32 value_hash;
};

} // namespace noir::jmt
