// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/merkle/proof.h>

namespace noir::consensus::merkle {

constexpr int max_aunts{100};

std::pair<std::vector<std::shared_ptr<proof_node>>, std::shared_ptr<proof_node>> trails_from_bytes_list(
  const bytes_list& items) {
  switch (items.size()) {
  case 0:
    return {{}, std::make_shared<proof_node>(proof_node{get_empty_hash()})};
  case 1: {
    auto trail = std::make_shared<proof_node>(proof_node{leaf_hash_opt(items[0])});
    return {{trail}, trail};
  }
  }
  auto k = get_split_point(items.size());
  auto [lefts, left_root] = trails_from_bytes_list({items.begin(), items.begin() + k});
  auto [rights, right_root] = trails_from_bytes_list({items.begin() + k, items.end()});
  auto root_hash = inner_hash_opt(left_root->hash, right_root->hash);
  auto root = std::make_shared<proof_node>(proof_node{root_hash});
  left_root->parent = root;
  left_root->right = right_root;
  right_root->parent = root;
  right_root->left = left_root;
  std::vector<std::shared_ptr<proof_node>> left_right;
  left_right.reserve(lefts.size() + rights.size() + 1);
  left_right.insert(left_right.end(), lefts.begin(), lefts.end());
  left_right.insert(left_right.end(), rights.begin(), rights.end());
  return {left_right, root};
}

std::pair<bytes, std::vector<std::shared_ptr<proof>>> proofs_from_bytes_list(const bytes_list& items) {
  auto [trails, root_spn] = trails_from_bytes_list(items);
  bytes root_hash = root_spn->hash;
  std::vector<std::shared_ptr<proof>> proofs;
  proofs.resize(items.size());
  for (auto i = 0; i < trails.size(); i++) {
    proofs[i] = std::make_shared<proof>(
      proof{static_cast<int64_t>(items.size()), i, trails[i]->hash, trails[i]->flatten_aunts()});
  }
  return {root_hash, proofs};
}

} // namespace noir::consensus::merkle
