// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/jmt/types/node.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace noir::jmt {

template<typename T>
struct mock_tree_store : public tree_reader<T>, public tree_writer<T> {
  auto get_node_option(const jmt::node_key& node_key) -> Result<std::optional<node<T>>> override {
    std::shared_lock _{data.lock};
    if (data._0.contains(node_key)) {
      return data._0.at(node_key);
    }
    return success();
  }

  auto get_rightmost_leaf() -> Result<std::optional<std::pair<jmt::node_key, leaf_node<T>>>> override {
    std::shared_lock _{data.lock};
    auto node_key_and_node = std::optional<std::pair<node_key, leaf_node<T>>>{};
    for (const auto& i : data._0) {
      if (i.second.is_leaf()) {
        auto leaf_node = std::get<jmt::leaf_node<T>>(i.second.data);
        if (!node_key_and_node || (node_key_and_node->second.account_key < leaf_node.account_key)) {
          node_key_and_node = {i.first, leaf_node};
        }
      }
    }
    return node_key_and_node;
  }

  auto write_node_batch(const jmt::node_batch<T>& node_batch) -> Result<void> override {
    std::unique_lock _{data.lock};
    for (const auto& [node_key, node] : node_batch) {
      check(allow_overwrite || !data._0.contains(node_key));
      data._0.insert_or_assign(node_key, node);
    }
    return success();
  }

  auto put_node(const jmt::node_key& node_key, const jmt::node<T>& node) -> Result<void> {
    std::unique_lock _{data.lock};
    noir_ensure(!data._0.contains(node_key), "key {} exists", node_key.to_string());
    data._0.insert({node_key, node});
    return success();
  }

  auto put_stale_node_index(const stale_node_index& index) -> Result<void> {
    std::unique_lock _{data.lock};
    check(!data._1.contains(index), "duplicated retire log");
    data._1.insert(index);
    return success();
  }

  auto write_tree_update_batch(const tree_update_batch<T>& batch) -> Result<void> {
    for (const auto& [k, v] : batch.node_batch) {
      noir_ok(put_node(k, v));
    }
    for (const auto& i : batch.stale_node_index_batch) {
      noir_ok(put_stale_node_index(i));
    }
    return success();
  }

  auto purge_stale_nodes(version least_readable_version) -> Result<void> {
    std::unique_lock _{data.lock};
    std::vector<stale_node_index> to_prune;
    for (auto it = data._1.begin(); it != data._1.end(); ++it) {
      if (it->stale_since_version > least_readable_version) {
        break;
      }
      to_prune.push_back(*it);
    }
    for (const auto& log : to_prune) {
      auto removed = data._0.contains(log.node_key);
      data._0.erase(log.node_key);
      noir_ensure(removed, "stale node index refers to non-existent node");
      data._1.erase(log);
    }
    return success();
  }

  auto num_nodes() {
    std::shared_lock _{data.lock};
    return data._0.size();
  }

  struct {
    std::shared_mutex lock;
    std::unordered_map<node_key, node<T>> _0;
    std::set<stale_node_index> _1;
  } data;
  bool allow_overwrite;
};

} // namespace noir::jmt
