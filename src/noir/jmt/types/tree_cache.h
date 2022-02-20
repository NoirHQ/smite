// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/jmt/types/common.h>
#include <noir/jmt/types/node.h>

namespace noir::jmt {

template<typename T>
struct frozen_tree_cache {
  node_batch<T> node_cache;
  stale_node_index_batch stale_node_index_cache;
  std::vector<jmt::node_stats> node_stats;
  std::vector<bytes32> root_hashes;
};

extern jmt::version pre_genesis_version;

template<typename R, typename T>
struct tree_cache {
  tree_cache(tree_cache&&) = default;
  tree_cache(const tree_cache&) = default;
  tree_cache& operator=(const tree_cache&) = default;

  tree_cache(R& reader, version next_version = 0): reader(reader), next_version(next_version) {
    if (!next_version) {
      auto pre_genesis_root_key = node_key{pre_genesis_version, {}};
      auto pre_genesis_root = reader.get_node_option(pre_genesis_root_key);
      if (pre_genesis_root) {
        root_node_key = pre_genesis_root_key;
      } else {
        auto genesis_root_key = node_key{0, {}};
        node_cache.insert({genesis_root_key, {null{}}});
        root_node_key = genesis_root_key;
      }
    } else {
      root_node_key = node_key{next_version - 1, {}};
    }
  }

  result<node<T>> get_node(const jmt::node_key& node_key) {
    auto it = node_cache.find(node_key);
    if (it != node_cache.end()) {
      return it->second;
    }
    it = frozen_cache.node_cache.find(node_key);
    if (it != frozen_cache.node_cache.end()) {
      return it->second;
    }
    // TODO: metric
    return reader.get_node(node_key);
  }

  result<void> put_node(const jmt::node_key& node_key, const node<T>& new_node) {
    if (!node_cache.contains(node_key)) {
      if (new_node.is_leaf())
        num_new_leaves += 1;
      node_cache.insert({node_key, new_node});
    } else {
      // TODO: error message
      return unexpected(error{"node with key already exists in node_batch"});
    }
    return {};
  }

  void delete_node(const node_key& old_node_key, bool is_leaf) {
    auto it = node_cache.find(old_node_key);
    if (it == node_cache.end()) {
      check(!stale_node_index_cache.contains(old_node_key), "node gets stale twice unexpectedly");
      stale_node_index_cache.insert(old_node_key);
      if (is_leaf)
        num_stale_leaves += 1;
    } else {
      node_cache.erase(it);
      num_new_leaves -= 1;
    }
  }

  void freeze() {
    auto root_node = get_node(root_node_key);
    // TODO: error message
    check(root_node, fmt::format("root node with key must exist"));
    auto root_hash = (*root_node).hash();
    frozen_cache.root_hashes.push_back(root_hash);
    jmt::node_stats node_stats {
      node_cache.size(),
      num_new_leaves,
      stale_node_index_cache.size(),
      num_stale_leaves,
    };
    frozen_cache.node_stats.push_back(node_stats);
    frozen_cache.node_cache.insert(frozen_cache.node_cache.end(), node_cache.begin(), node_cache.end());
    node_cache.clear();
    auto stale_since_version = next_version;
    std::transform(
      frozen_cache.stale_node_index_cache.begin(),
      frozen_cache.stale_node_index_cache.end(),
      frozen_cache.stale_node_index_cache.begin(),
      [=](const auto& s) { return stale_node_index{stale_since_version, s.node_key}; });
    num_stale_leaves = 0;
    num_new_leaves = 0;
    next_version += 1;
  }

  operator std::pair<std::vector<bytes32>, tree_update_batch<T>>() {
    return {
      frozen_cache.root_hashes,
      {frozen_cache.node_cache, frozen_cache.stale_node_index_cache, frozen_cache.node_stats},
    };
  }

  node_key root_node_key;
  version next_version;
  std::map<node_key, node<T>> node_cache;
  size_t num_new_leaves = 0;
  std::set<node_key> stale_node_index_cache;
  size_t num_stale_leaves = 0;
  frozen_tree_cache<T> frozen_cache;
  R& reader;
};

} // namespace noir::jmt
