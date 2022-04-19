// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/consensus/merkle/tree.h>
#include <optional>

namespace noir::consensus::merkle {

struct proof {
  int64_t total{};
  int64_t index{};
  bytes leaf_hash{};
  bytes_list aunts{};

  std::optional<std::string> verify(const bytes& root_hash, const bytes& leaf) {
    if (total < 0)
      return "proof total must be positive";
    if (index < 0)
      return "proof index must be positive";
    auto leaf_hash_ = leaf_hash_opt(leaf);
    if (leaf_hash_ != leaf_hash)
      return "invalid leaf hash";
    auto computed_hash = compute_root_hash();
    if (computed_hash != root_hash)
      return "invalid root hash";
    return {};
  }

  bytes compute_root_hash() const {
    return compute_hash_from_aunts(index, total, leaf_hash, aunts);
  }
};

struct proof_node {
  bytes hash{};
  std::shared_ptr<proof_node> parent{};
  std::shared_ptr<proof_node> left{};
  std::shared_ptr<proof_node> right{};

  /// \brief returns inner hashes for the item corresponding to leaf, starting from a leaf proof_node
  bytes_list flatten_aunts() {
    if (!parent && !left && !right)
      return {};
    bytes_list inner_hashes{};
    proof_node* cur = this;
    while (cur != nullptr) {
      if (cur->left != nullptr)
        inner_hashes.push_back(cur->left->hash);
      if (cur->right != nullptr)
        inner_hashes.push_back(cur->right->hash);
      cur = cur->parent.get();
    }
    return inner_hashes;
  }
};

/// \brief recursively constructs merkle proofs
std::pair<std::vector<std::shared_ptr<proof_node>>, std::shared_ptr<proof_node>> trails_from_bytes_list(
  const bytes_list& items);

/// \brief computes inclusion proof for given list
/// \param items list of items used for generating proof
/// \return list of proofs; proof[0] is the proof for list[0]
std::pair<bytes, std::vector<std::shared_ptr<proof>>> proofs_from_bytes_list(const bytes_list& items);

} // namespace noir::consensus::merkle

NOIR_REFLECT(noir::consensus::merkle::proof, total, index, leaf_hash, aunts);
