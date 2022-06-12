// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/jmt/types.h>
#include <iosfwd>
#include <unordered_map>

namespace noir::jmt {

template<typename R, typename T = typename R::value_type>
struct jellyfish_merkle_tree {
  jellyfish_merkle_tree(R& reader): reader(reader) {}

  static jmt::nibble nibble(const Bytes32& bytes, size_t index) {
    auto upper = !(index % 2);
    return upper ? ((bytes[index / 2] & 0xf0) >> 4) : bytes[index / 2] & 0x0f;
  }

  struct nibble_range_iterator {
  private:
    nibble_range_iterator(std::span<std::pair<Bytes32, T>> sorted_kvs, size_t nibble_idx)
      : sorted_kvs(sorted_kvs), nibble_idx(nibble_idx) {
      check(nibble_idx < root_nibble_height);
    }

    friend class jmt::jellyfish_merkle_tree<R, T>;

  public:
    using item_type = std::pair<size_t, size_t>;

    nibble_range_iterator(const nibble_range_iterator& it)
      : sorted_kvs(it.sorted_kvs), nibble_idx(it.nibble_idx), pos(it.pos) {}

    nibble_range_iterator& operator=(const nibble_range_iterator& it) {
      if (this != &it)
        *this = it;
      return *this;
    }

    std::optional<item_type> next() {
      auto left = pos;
      if (pos < sorted_kvs.size()) {
        uint8_t cur_nibble = nibble(std::get<0>(sorted_kvs[left]), nibble_idx);
        auto i = left;
        auto j = sorted_kvs.size() - 1;
        while (i < j) {
          auto mid = j - (j - i) / 2;
          if (nibble(std::get<0>(sorted_kvs[mid]), nibble_idx) > cur_nibble) {
            j = mid - 1;
          } else {
            i = mid;
          }
        }
        pos = i + 1;
        return std::make_pair(left, i);
      }
      return {};
    }

    std::span<std::pair<Bytes32, T>> sorted_kvs;
    size_t nibble_idx;
    size_t pos = 0;
    std::optional<item_type> item;
  };

  auto get_hash(const jmt::node_key& node_key,
    const jmt::node<T>& node,
    const std::optional<std::unordered_map<nibble_path, Bytes32>>& hash_cache) -> Bytes32 {
    if (hash_cache) {
      auto it = hash_cache->find(node_key.nibble_path);
      if (it != hash_cache->end()) {
        return it->second;
      }
      check(false, fmt::format("{} cannot be found in hash_cache", node_key.to_string()));
      __builtin_unreachable();
    } else {
      return node.hash();
    }
  }

  auto batch_put_value_sets(const std::vector<std::vector<std::pair<Bytes32, T>>>& value_sets,
    std::optional<std::vector<std::reference_wrapper<std::unordered_map<jmt::nibble_path, Bytes32>>>> node_hashes,
    jmt::version first_version) -> result<std::pair<std::vector<Bytes32>, tree_update_batch<T>>> {
    auto tree_cache = jmt::tree_cache(reader, first_version);
    std::vector<std::optional<std::reference_wrapper<std::unordered_map<jmt::nibble_path, Bytes32>>>> hash_sets;
    if (node_hashes) {
      for (const auto& hashes : *node_hashes) {
        hash_sets.push_back(hashes);
      }
    } else {
      hash_sets = {value_sets.size(), std::nullopt};
    }
    auto value_set = value_sets.begin();
    auto hash_set = hash_sets.begin();
    for (auto idx = 0; value_set != value_sets.end() && hash_set != hash_sets.end(); ++value_set, ++hash_set, ++idx) {
      check(value_set->size(), "transactions that output empty write set should not be included");
      auto version = first_version + idx;
      std::map<Bytes32, T> sorted_kvs;
      for (const auto& [k, v] : *value_set) {
        sorted_kvs.insert_or_assign(k, v);
      }
      std::vector<std::pair<Bytes32, T>> deduped_and_sorted_kvs{sorted_kvs.begin(), sorted_kvs.end()};
      auto root_node_key = tree_cache.root_node_key;
      auto [new_root_node_key, _] =
        noir_ok(batch_insert_at(root_node_key, version, std::span(deduped_and_sorted_kvs), 0, *hash_set, tree_cache));
      tree_cache.root_node_key = new_root_node_key;
      tree_cache.freeze();
    }
    check(value_set == value_sets.end() && hash_set == hash_sets.end());
    return tree_cache.deltas();
  }

  auto batch_insert_at(jmt::node_key& node_key,
    jmt::version version,
    std::span<std::pair<Bytes32, T>> kvs,
    size_t depth,
    std::optional<std::reference_wrapper<std::unordered_map<nibble_path, Bytes32>>> hash_cache,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    check(kvs.size());
    auto node = noir_ok(tree_cache.get_node(node_key));
    return std::visit(
      overloaded{
        [&](jmt::internal_node& internal_node) -> result<std::pair<jmt::node_key, jmt::node<T>>> {
          tree_cache.delete_node(node_key, false);
          auto children = internal_node.children;
          auto it = nibble_range_iterator(kvs, depth);
          while (auto v = it.next()) {
            auto [left, right] = *v;
            auto child_index = nibble(std::get<0>(kvs[left]), depth);
            auto child = internal_node.child(child_index);
            jmt::node_key new_child_node_key;
            jmt::node<T> new_child_node;
            if (child) {
              auto child_node_key = node_key.gen_child_node_key(child->get().version, child_index);
              std::tie(new_child_node_key, new_child_node) = noir_ok(batch_insert_at(
                child_node_key, version, kvs.subspan(left, (right - left + 1)), depth + 1, hash_cache, tree_cache));
            } else {
              auto new_child_node_key_ = node_key.gen_child_node_key(version, child_index);
              std::tie(new_child_node_key, new_child_node) = noir_ok(batch_create_subtree(new_child_node_key_, version,
                kvs.subspan(left, (right - left + 1)), depth + 1, hash_cache, tree_cache));
            }
            children.insert_or_assign(child_index,
              jmt::child{
                get_hash(new_child_node_key, new_child_node, hash_cache), version, new_child_node.node_type()});
          }
          auto new_internal_node = jmt::internal_node(children);
          node_key.version = version;
          noir_ok(tree_cache.put_node(node_key, {new_internal_node}));
          return {{node_key, {new_internal_node}}};
        },
        [&](jmt::leaf_node<T>& leaf_node) -> result<std::pair<jmt::node_key, jmt::node<T>>> {
          tree_cache.delete_node(node_key, true);
          node_key.version = version;
          return batch_create_subtree_with_existing_leaf(
            node_key, version, leaf_node, kvs, depth, hash_cache, tree_cache);
        },
        [&](jmt::null&) -> result<std::pair<jmt::node_key, jmt::node<T>>> {
          if (!node_key.nibble_path.is_empty()) {
            noir_bail("null node exists for non-root node with node_key {}", node_key.to_string());
          }
          if (node_key.version == version) {
            tree_cache.delete_node(node_key, false);
          }
          return batch_create_subtree(jmt::node_key{version}, version, kvs, depth, hash_cache, tree_cache);
        },
      },
      node.data);
  }

  auto batch_create_subtree_with_existing_leaf(const jmt::node_key& node_key,
    jmt::version version,
    jmt::leaf_node<T> existing_leaf_node,
    std::span<std::pair<Bytes32, T>> kvs,
    size_t depth,
    std::optional<std::reference_wrapper<std::unordered_map<nibble_path, Bytes32>>> hash_cache,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    auto existing_leaf_key = existing_leaf_node.account_key;
    if (kvs.size() == 1 && std::get<0>(kvs[0]) == existing_leaf_key) {
      auto new_leaf_node = node<T>::leaf(existing_leaf_key, std::get<1>(kvs[0]));
      noir_ok(tree_cache.put_node(node_key, new_leaf_node));
      return std::make_pair(node_key, new_leaf_node);
    } else {
      auto existing_leaf_bucket = nibble(existing_leaf_key, depth);
      auto isolated_existing_leaf = true;
      jmt::children children;
      auto it = nibble_range_iterator(kvs, depth);
      while (auto v = it.next()) {
        auto [left, right] = *v;
        auto child_index = nibble(std::get<0>(kvs[left]), depth);
        auto child_node_key = node_key.gen_child_node_key(version, child_index);
        jmt::node_key new_child_node_key;
        jmt::node<T> new_child_node;
        if (existing_leaf_bucket == child_index) {
          isolated_existing_leaf = false;
          std::tie(new_child_node_key, new_child_node) = noir_ok(batch_create_subtree_with_existing_leaf(child_node_key,
            version, existing_leaf_node, kvs.subspan(left, (right - left + 1)), depth + 1, hash_cache, tree_cache));
        } else {
          std::tie(new_child_node_key, new_child_node) = noir_ok(batch_create_subtree(
            child_node_key, version, kvs.subspan(left, (right - left + 1)), depth + 1, hash_cache, tree_cache));
        }
        children.insert_or_assign(child_index,
          jmt::child{get_hash(new_child_node_key, new_child_node, hash_cache), version, new_child_node.node_type()});
      }
      if (isolated_existing_leaf) {
        auto existing_leaf_node_key = node_key.gen_child_node_key(version, existing_leaf_bucket);
        children.insert_or_assign(existing_leaf_bucket, jmt::child{existing_leaf_node.hash(), version, leaf{}});
        noir_ok(tree_cache.put_node(existing_leaf_node_key, {existing_leaf_node}));
      }
      auto new_internal_node = jmt::internal_node(std::move(children));
      noir_ok(tree_cache.put_node(node_key, {new_internal_node}));
      return {{node_key, {new_internal_node}}};
    }
  }

  auto batch_create_subtree(const jmt::node_key& node_key,
    jmt::version version,
    std::span<std::pair<Bytes32, T>> kvs,
    size_t depth,
    std::optional<std::reference_wrapper<std::unordered_map<nibble_path, Bytes32>>> hash_cache,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    if (kvs.size() == 1) {
      auto new_leaf_node = node<T>::leaf(std::get<0>(kvs[0]), std::get<1>(kvs[0]));
      noir_ok(tree_cache.put_node(node_key, new_leaf_node));
      return std::make_pair(node_key, new_leaf_node);
    } else {
      jmt::children children;
      auto it = nibble_range_iterator(kvs, depth);
      while (auto v = it.next()) {
        auto [left, right] = *v;
        auto child_index = nibble(std::get<0>(kvs[left]), depth);
        auto child_node_key = node_key.gen_child_node_key(version, child_index);
        auto [new_child_node_key, new_child_node] = noir_ok(batch_create_subtree(
          child_node_key, version, kvs.subspan(left, (right - left + 1)), depth + 1, hash_cache, tree_cache));
        children.insert_or_assign(child_index,
          jmt::child{get_hash(new_child_node_key, new_child_node, hash_cache), version, new_child_node.node_type()});
      }
      auto new_internal_node = jmt::internal_node(std::move(children));
      noir_ok(tree_cache.put_node(node_key, {new_internal_node}));
      return {{node_key, {new_internal_node}}};
    }
  }

  auto put(const Bytes32& key, const T& value, jmt::version version, jmt::tree_cache<R, T>& tree_cache)
    -> result<void> {
    auto nibble_path = jmt::nibble_path(std::span(key));
    auto root_node_key = tree_cache.root_node_key;
    auto nibble_iter = nibble_path.nibbles();
    auto [new_root_node_key, _] = noir_ok(insert_at(root_node_key, version, nibble_iter, value, tree_cache));
    tree_cache.root_node_key = new_root_node_key;
    return {};
  }

  auto insert_at(jmt::node_key& node_key,
    jmt::version version,
    nibble_path::nibble_iterator& nibble_iter,
    const T& value,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, jmt::node<T>>> {
    auto node = noir_ok(tree_cache.get_node(node_key));
    return std::visit(overloaded{[&](jmt::internal_node& internal_node) {
                                   return insert_at_internal_node(
                                     node_key, internal_node, version, nibble_iter, value, tree_cache);
                                 },
                        [&](jmt::leaf_node<T>& leaf_node) {
                          return insert_at_leaf_node(node_key, leaf_node, version, nibble_iter, value, tree_cache);
                        },
                        [&](jmt::null& null) -> result<std::pair<jmt::node_key, jmt::node<T>>> {
                          if (!node_key.nibble_path.is_empty()) {
                            // TODO: add node_key contents to error message
                            return make_unexpected(fmt::format("null node exists for non-root node with node key"));
                          }
                          if (node_key.version == version) {
                            tree_cache.delete_node(node_key, false);
                          }
                          return create_leaf_node(jmt::node_key{version, {}}, nibble_iter, value, tree_cache);
                        }},
      node.data);
  }

  auto insert_at_internal_node(jmt::node_key& node_key,
    jmt::internal_node& internal_node,
    jmt::version version,
    nibble_path::nibble_iterator& nibble_iter,
    const T& value,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    tree_cache.delete_node(node_key, false);
    auto child_index = expect(nibble_iter.next(), "ran out of nibbles");
    auto child = internal_node.child(child_index);
    auto [_, new_child_node] = noir_ok((child)
    ? [&]() {
        auto child_node_key = node_key.gen_child_node_key(child->get().version, child_index);
        return insert_at(child_node_key, version, nibble_iter, value, tree_cache);
      }()
    : [&]() {
        auto new_child_node_key = node_key.gen_child_node_key(version, child_index);
        return create_leaf_node(new_child_node_key, nibble_iter, value, tree_cache);
      }());
    auto& children = internal_node.children;
    children.insert_or_assign(child_index, jmt::child{new_child_node.hash(), version, new_child_node.node_type()});
    auto new_internal_node = jmt::internal_node(children);
    node_key.version = version;
    tree_cache.put_node(node_key, {new_internal_node});
    return {{node_key, {new_internal_node}}};
  }

  auto insert_at_leaf_node(jmt::node_key& node_key,
    jmt::leaf_node<T>& existing_leaf_node,
    jmt::version version,
    nibble_path::nibble_iterator& nibble_iter,
    const T& value,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    tree_cache.delete_node(node_key, true);
    auto visited_nibble_iter = nibble_iter.visited_nibbles();
    auto existing_leaf_nibble_path = jmt::nibble_path(std::span(existing_leaf_node.account_key));
    auto existing_leaf_nibble_iter = existing_leaf_nibble_path.nibbles();
    skip_common_prefix(visited_nibble_iter, existing_leaf_nibble_iter);
    check(visited_nibble_iter.is_finished(),
      fmt::format("leaf nodes failed to share the same visited nibbles before index {}",
        existing_leaf_nibble_iter.visited_nibbles().num_nibbles()));
    auto existing_leaf_nibble_iter_below_internal = existing_leaf_nibble_iter.remaining_nibbles();
    auto num_common_nibbles_below_internal = skip_common_prefix(nibble_iter, existing_leaf_nibble_iter_below_internal);
    jmt::nibble_path common_nibble_path;
    auto visited = nibble_iter.visited_nibbles();
    while (auto n = visited.next()) {
      common_nibble_path.push(*n);
    }
    if (nibble_iter.is_finished()) {
      check(existing_leaf_nibble_iter_below_internal.is_finished());
      node_key.version = version;
      return create_leaf_node(node_key, nibble_iter, value, tree_cache);
    }
    auto existing_leaf_index = expect(existing_leaf_nibble_iter_below_internal.next(), "ran out of nibbles");
    auto new_leaf_index = expect(nibble_iter.next(), "ran out of nibbles");
    check(existing_leaf_index != new_leaf_index);
    jmt::children children;
    children.insert_or_assign(existing_leaf_index, child{existing_leaf_node.hash(), version, leaf{}});
    node_key = jmt::node_key{version, common_nibble_path};
    noir_ok(tree_cache.put_node(node_key.gen_child_node_key(version, existing_leaf_index), {existing_leaf_node}));
    auto [_, new_leaf_node] =
      noir_ok(create_leaf_node(node_key.gen_child_node_key(version, new_leaf_index), nibble_iter, value, tree_cache));
    children.insert_or_assign(new_leaf_index, child{new_leaf_node.hash(), version, leaf{}});
    auto internal_node = jmt::internal_node(children);
    auto next_internal_node = internal_node;
    noir_ok(tree_cache.put_node(node_key, {internal_node}));
    for (auto i = 0; i < num_common_nibbles_below_internal; ++i) {
      auto nibble = expect(common_nibble_path.pop(), "common nibble_path below internal node ran out of nibble");
      node_key = jmt::node_key{version, common_nibble_path};
      jmt::children children;
      children.insert_or_assign(nibble, child{next_internal_node.hash(), version, next_internal_node.node_type()});
      auto internal_node = jmt::internal_node(std::move(children));
      next_internal_node = internal_node;
      noir_ok(tree_cache.put_node(node_key, {internal_node}));
    }
    return {{node_key, {next_internal_node}}};
  }

  auto create_leaf_node(const jmt::node_key& node_key,
    nibble_path::nibble_iterator& nibble_iter,
    const T& value,
    jmt::tree_cache<R, T>& tree_cache) -> result<std::pair<jmt::node_key, node<T>>> {
    auto new_leaf_node = node<T>::leaf(nibble_iter.nibble_path.bytes, value);
    noir_ok(tree_cache.put_node(node_key, new_leaf_node));
    return {{node_key, {new_leaf_node}}};
  }

  auto get_range_proof() {}

  auto get(const Bytes32& key, jmt::version version) -> result<std::optional<T>> {
    return {std::get<0>(noir_ok(get_with_proof(key, version)))};
  }

  auto get_root_node(jmt::version version) -> result<node<T>> {
    auto root_node_opt = get_root_node_option(version);
    noir_ensure(root_node_opt && *root_node_opt, "root node not found for version {}", version);
    return **root_node_opt;
  }

  auto get_root_node_option(jmt::version version) -> result<std::optional<node<T>>> {
    return reader.get_node_option(node_key{version});
  }

  auto get_root_hash(jmt::version version) -> result<Bytes32> {
    return get_root_node(version)->hash();
  }

  auto get_root_hash_option(jmt::version version) -> result<std::optional<Bytes32>> {
    auto root_node_opt = get_root_node_option(version);
    if (!root_node_opt) {
      return make_unexpected(root_node_opt.error());
    }
    if (*root_node_opt) {
      return root_node_opt->value().hash();
    }
    return std::nullopt;
  }

  auto get_leaf_count(jmt::version version) -> result<std::optional<size_t>> {
    auto root_node_opt = get_root_node_option(version);
    if (!root_node_opt) {
      return make_unexpected(root_node_opt.error());
    }
    if (*root_node_opt) {
      return root_node_opt->value().leaf_count();
    }
    return std::nullopt;
  }

  auto put_value_set(const std::vector<std::pair<Bytes32, T>>& value_set, jmt::version version)
    -> result<std::pair<Bytes32, tree_update_batch<T>>> {
    auto [root_hashes, tree_update_batch] = noir_ok(batch_put_value_sets({value_set}, {}, version));
    check(root_hashes.size() == 1, "root_hashes must consist of a single value");
    return std::make_pair(root_hashes[0], tree_update_batch);
  }

  auto put_value_sets(const std::vector<std::vector<std::pair<Bytes32, T>>>& value_sets, jmt::version first_version)
    -> result<std::pair<std::vector<Bytes32>, tree_update_batch<T>>> {
    auto tree_cache = jmt::tree_cache(reader, first_version);
    for (auto idx = 0; const auto& value_set : value_sets) {
      check(value_set.size(), "transactions that output empty write set should not be included");
      auto version = first_version + idx;
      for (const auto& [key, value] : value_set) {
        noir_ok(put(key, value, version, tree_cache));
      }
      tree_cache.freeze();
      ++idx;
    }
    return tree_cache.deltas();
  }

  auto get_with_proof(const Bytes32& key, jmt::version version)
    -> result<std::pair<std::optional<T>, sparse_merkle_proof<T>>> {
    auto next_node_key = node_key{version};
    auto siblings = std::vector<Bytes32>();
    auto nibble_path = jmt::nibble_path(std::span(key));
    auto nibble_iter = nibble_path.nibbles();

    for (auto nibble_depth = 0; nibble_depth <= root_nibble_height; ++nibble_depth) {
      auto next_node = reader.get_node(next_node_key);
      if (!next_node) {
        if (!nibble_depth) {
          return make_unexpected(fmt::format("missing state root node at version {}, probably pruned", version));
        } else {
          return make_unexpected(next_node.error());
        }
      } else {
        if (std::holds_alternative<jmt::internal_node>(next_node.value().data)) {
          auto internal_node = std::get<jmt::internal_node>(next_node.value().data);
          auto queried_child_index = expect(nibble_iter.next(), "ran out of nibbles");
          auto [child_node_key, siblings_in_internal] =
            internal_node.get_child_with_siblings(next_node_key, queried_child_index);
          siblings.insert(siblings.end(), siblings_in_internal.begin(), siblings_in_internal.end());
          if (child_node_key) {
            next_node_key = *child_node_key;
          } else {
            return std::make_pair(std::nullopt,
              sparse_merkle_proof<T>{std::nullopt, (reverse(siblings.begin(), siblings.end()), siblings)});
          }
        } else if (std::holds_alternative<jmt::leaf_node<T>>(next_node.value().data)) {
          auto leaf_node = std::get<jmt::leaf_node<T>>(next_node.value().data);
          return std::make_pair((leaf_node.account_key == key) ? std::optional{leaf_node.value} : std::nullopt,
            sparse_merkle_proof<T>{sparse_merkle_leaf_node{leaf_node.account_key, leaf_node.value_hash},
              (reverse(siblings.begin(), siblings.end()), siblings)});
        } else {
          if (!nibble_depth) {
            return std::make_pair(std::nullopt, sparse_merkle_proof<T>{});
          } else {
            noir_bail("non-root null node exists with node key _key_");
          }
        }
      }
    }
    noir_bail("jellyfish merkle tree has cyclic graph inside");
  }

  void traverse_node(jmt::tree_cache<R, T>& tree_cache, const node_key& key, int depth) {
    std::cout << std::string(depth * 2, ' ') << key.to_string() << " ";
    auto node = tree_cache.get_node(key);
    if (!node) {
      std::cout << "ERROR" << std::endl;
      return;
    }
    if (std::holds_alternative<jmt::internal_node>(node->data)) {
      auto internal_node = std::get<jmt::internal_node>(node->data);
      std::cout << fmt::format(
                     "internal{{leaf_count: {}}} {}", internal_node.leaf_count, internal_node.hash().to_string())
                << std::endl;
      for (const auto& c : internal_node.children) {
        traverse_node(tree_cache, key.gen_child_node_key(c.second.version, c.first), depth + 1);
      }
    } else if (std::holds_alternative<jmt::leaf_node<T>>(node->data)) {
      auto leaf_node = std::get<jmt::leaf_node<T>>(node->data);
      std::cout << fmt::format("leaf{{value: {}}} {}", to_string(leaf_node.value), leaf_node.hash().to_string())
                << std::endl;
    } else if (std::holds_alternative<jmt::null>(node->data)) {
      std::cout << "null{} " << sparse_merkle_placeholder_hash.to_string() << std::endl;
    }
  }

  void debug(jmt::version version) {
    auto tree_cache = jmt::tree_cache(reader, version);
    traverse_node(tree_cache, tree_cache.root_node_key, 0);
  }

private:
  R& reader;
};

} // namespace noir::jmt
