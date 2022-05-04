// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/overloaded.h>
#include <noir/common/types/bytes.h>
#include <noir/codec/bcs.h>
#include <noir/crypto/hash/xxhash.h>
#include <noir/jmt/types/common.h>
#include <noir/jmt/types/nibble.h>
#include <noir/jmt/types/proof.h>
#include <fmt/core.h>
#include <bit>
#include <map>
#include <set>
#include <unordered_map>

namespace noir::jmt {

using codec::bcs::datastream;

namespace detail {
  inline void serialize_u64_varint(uint64_t num, std::vector<uint8_t>& binary) {
    auto encoded = codec::bcs::encode(varuint64(num));
    binary.insert(binary.end(), encoded.begin(), encoded.end());
  }

  inline uint64_t deserialize_u64_varint(std::span<const char> binary) {
    datastream<const char> ds(binary);
    varuint64 v;
    ds >> v;
    return v.value;
  }
} // namespace detail

struct node_key {
  node_key gen_child_node_key(version v, nibble n) const {
    node_key child = *this;
    child.version = v;
    child.nibble_path.push(n);
    return child;
  }

  node_key gen_parent_node_key() const {
    node_key parent = *this;
    auto popped = parent.nibble_path.pop();
    check(popped.has_value(), "current node key is root");
    return parent;
  }

  std::vector<uint8_t> encode() const {
    std::vector<uint8_t> out(sizeof(version) + 1 + nibble_path.bytes.size());
    std::reverse_copy((char*)&version, (char*)&version + sizeof(version), out.begin());
    out[sizeof(version)] = nibble_path.num_nibbles;
    std::copy(nibble_path.bytes.begin(), nibble_path.bytes.end(), out.begin() + sizeof(version) + 1);
    return out;
  }

  // TODO: return type result
  static node_key decode(std::span<uint8_t> val) {
    node_key out;
    std::reverse_copy(val.begin(), val.begin() + sizeof(version), (char*)&out.version);
    out.nibble_path.num_nibbles = val[sizeof(version)];
    out.nibble_path.bytes = {val.begin() + sizeof(version) + 1, val.end()};
    if (out.nibble_path.num_nibbles % 2) {
      auto padding = out.nibble_path.bytes.back() & 0x0f;
      check(!padding, fmt::format("padding nibble expected to be 0, got: {}", padding));
    }
    return out;
  }

  std::string to_string() const {
    return fmt::format("{{version: {}, nibble_path: {}}}", version, nibble_path.to_string());
  }

  jmt::version version;
  jmt::nibble_path nibble_path;
};

inline bool operator==(const node_key& a, const node_key& b) {
  return std::tie(a.version, a.nibble_path) == std::tie(b.version, b.nibble_path);
}

inline bool operator<(const node_key& a, const node_key& b) {
  return std::tie(a.version, a.nibble_path) < std::tie(b.version, b.nibble_path);
}

struct leaf {};
struct internal {
  size_t leaf_count;
};

using node_type = std::variant<leaf, /* internal_legacy, */ internal>;

inline bool operator==(const node_type& a, const node_type& b) {
  if (a.index() != b.index())
    return false;
  if (a.index() == 1)
    return std::get<1>(a).leaf_count == std::get<1>(b).leaf_count;
  return true;
}

struct child {
  bool is_leaf() const {
    return std::holds_alternative<leaf>(node_type);
  }

  size_t leaf_count() const {
    return std::visit(overloaded{
                        [&](const leaf&) -> size_t { return 1; },
                        [&](const internal& n) { return n.leaf_count; },
                      },
      node_type);
  }

  bytes32 hash;
  jmt::version version;
  jmt::node_type node_type;

  friend bool operator==(const child& a, const child& b) {
    return std::tie(a.hash, a.version, a.node_type) == std::tie(b.hash, b.version, b.node_type);
  }
};

// for deterministic serialization
using children = std::unordered_map<nibble, child>;

inline bool operator==(const children::value_type& a, const children::value_type& b) {
  return std::tie(a.first, a.second) == std::tie(b.first, b.second);
}

struct internal_node {
  internal_node() = default;

  internal_node(const jmt::children& ch): children(ch), leaf_count(sum_leaf_count(children)) {
    check(!children.empty(), "children must not be empty");
    if (children.size() == 1) {
      check(!children.begin()->second.is_leaf(), "if there's only one child, it must not be a leaf");
    }
  }

  internal_node(jmt::children&& ch): children(std::move(ch)), leaf_count(sum_leaf_count(children)) {
    check(!children.empty(), "children must not be empty");
    if (children.size() == 1) {
      check(!children.begin()->second.is_leaf(), "if there's only one child, it must not be a leaf");
    }
  }

  static size_t sum_leaf_count(const children& ch) {
    size_t leaf_count = 0;
    for (const auto& c : ch) {
      leaf_count += c.second.leaf_count();
    }
    return leaf_count;
  }

  jmt::node_type node_type() {
    return internal{leaf_count};
  }

  using bitmap_type = std::array<uint16_t, 2>;

  bitmap_type generate_bitmaps() const {
    auto existence_bitmap = uint16_t(0);
    auto leaf_bitmap = uint16_t(0);
    for (const auto& c : children) {
      auto i = c.first.value;
      existence_bitmap |= uint16_t(1) << i;
      if (c.second.is_leaf()) {
        leaf_bitmap |= uint16_t(1) << i;
      }
    }
    check((existence_bitmap | leaf_bitmap) == existence_bitmap);
    return {existence_bitmap, leaf_bitmap};
  }

  bytes32 hash() const {
    return merkle_hash(0, 16, generate_bitmaps());
  }

  // for deterministic serialization
  // children_sorted();

  void serialize(std::vector<uint8_t>& binary) const {
    auto [existence_bitmap, leaf_bitmap] = generate_bitmaps();
    binary.push_back(existence_bitmap & 0xff);
    binary.push_back(existence_bitmap >> 8);
    binary.push_back(leaf_bitmap & 0xff);
    binary.push_back(leaf_bitmap >> 8);
    auto count = std::popcount(existence_bitmap);
    for (auto i = 0; i < count; ++i) {
      auto next_child = std::countr_zero(existence_bitmap);
      auto child = children.at(next_child);
      detail::serialize_u64_varint(child.version, binary);
      binary.insert(binary.end(), child.hash.begin(), child.hash.end());
      if (std::holds_alternative<internal>(child.node_type)) {
        detail::serialize_u64_varint(leaf_count, binary);
      }
      existence_bitmap &= ~(1 << next_child);
    }
  }

  // TODO: return type result
  static internal_node deserialize(std::span<const char> data) {
    datastream<const char> ds(data);
    uint16_t existence_bitmap = 0;
    uint16_t leaf_bitmap = 0;
    ds >> existence_bitmap;
    ds >> leaf_bitmap;
    check(existence_bitmap, "no children found in internal node");
    check((existence_bitmap & leaf_bitmap) == leaf_bitmap,
      fmt::format("extra leaves: existing: {}, leaves: {}", existence_bitmap, leaf_bitmap));

    jmt::children children;
    auto count = std::popcount(existence_bitmap);
    for (auto i = 0; i < count; ++i) {
      auto next_child = std::countr_zero(existence_bitmap);
      varuint64 v;
      ds >> v;
      auto version = v.value;
      check(ds.remaining() >= 32,
        fmt::format("not enough bytes left, children: {}, bytes: {}", std::popcount(existence_bitmap), ds.remaining()));
      bytes32 hash;
      ds >> std::span(hash.data(), hash.size());

      auto child_bit = 1 << next_child;
      jmt::node_type type;
      if (leaf_bitmap & child_bit) {
        type = leaf{};
      } else {
        varuint64 leaf_count;
        ds >> leaf_count;
        type = internal{leaf_count};
      }
      children.insert_or_assign(next_child, jmt::child{hash, version, type});
      existence_bitmap &= ~child_bit;
    }
    check(!existence_bitmap);
    return {children};
  }

  std::optional<std::reference_wrapper<const jmt::child>> child(nibble n) const {
    auto ch = children.find(n);
    if (ch != children.end()) {
      return ch->second;
    }
    return std::nullopt;
  }

  static bitmap_type range_bitmaps(uint8_t start, uint8_t width, bitmap_type bitmaps) {
    check(start < 16 && std::has_single_bit(width) && start % width == 0);
    check(width <= 16 && (start + width) <= 16);
    auto mask = uint16_t(((uint32_t(1) << width) - 1) << start);
    return {uint16_t(bitmaps[0] & mask), uint16_t(bitmaps[1] & mask)};
  }

  bytes32 merkle_hash(uint8_t start, uint8_t width, bitmap_type bitmaps) const {
    auto [range_existence_bitmap, range_leaf_bitmap] = range_bitmaps(start, width, bitmaps);
    if (!range_existence_bitmap) {
      return sparse_merkle_placeholder_hash;
    } else if (width == 1 || (std::has_single_bit(range_existence_bitmap) && range_leaf_bitmap)) {
      auto only_child_index = std::countr_zero(range_existence_bitmap);
      return child(only_child_index)->get().hash;
    } else {
      auto left_child = merkle_hash(start, width / 2, {range_existence_bitmap, range_leaf_bitmap});
      auto right_child = merkle_hash(start + width / 2, width / 2, {range_existence_bitmap, range_leaf_bitmap});
      return sparse_merkle_internal_node{left_child, right_child}.hash();
    }
  }

  static std::array<uint8_t, 2> get_child_and_sibling_half_start(nibble n, uint8_t height) {
    uint8_t child_half_start = (0xff << height) & n.value;
    uint8_t sibling_half_start = child_half_start ^ (1 << height);
    return {child_half_start, sibling_half_start};
  }

  std::tuple<std::optional<node_key>, std::vector<bytes32>> get_child_with_siblings(const node_key& key, nibble n) {
    std::vector<bytes32> siblings;
    auto [existence_bitmap, leaf_bitmap] = generate_bitmaps();

    for (auto h = 3; h >= 0; --h) {
      auto width = 1 << h;
      auto [child_half_start, sibling_half_start] = get_child_and_sibling_half_start(n, h);
      siblings.push_back(merkle_hash(sibling_half_start, width, {existence_bitmap, leaf_bitmap}));
      auto [range_existence_bitmap, range_leaf_bitmap] =
        range_bitmaps(child_half_start, width, {existence_bitmap, leaf_bitmap});
      if (!range_existence_bitmap) {
        return {std::nullopt, siblings};
      } else if (width == 1 || (std::has_single_bit(range_existence_bitmap) && range_leaf_bitmap)) {
        auto only_child_index = nibble(std::countr_zero(range_existence_bitmap));
        return {
          [&]() -> node_key {
            auto only_child_version = child(only_child_index)->get().version;
            return key.gen_child_node_key(only_child_version, only_child_index);
          }(),
          siblings,
        };
      }
    }
    check(false, "impossible to get here without returning even at the lowest level");
    return {};
  }

  friend bool operator==(const internal_node& a, const internal_node& b) {
    return std::tie(a.children, a.leaf_count) == std::tie(b.children, b.leaf_count);
  }

  jmt::children children;
  size_t leaf_count;
};

inline std::array<uint8_t, 2> get_child_and_sibling_half_start(nibble n, uint8_t height) {
  uint8_t child_half_start = (0xff << height) & n.value;
  uint8_t sibling_half_start = child_half_start ^ (1 << height);
  return {child_half_start, sibling_half_start};
}

struct null {};

template<typename T>
struct leaf_node {
  leaf_node() = default;

  leaf_node(const bytes32& account_key, const T& value): account_key(account_key), value(value) {
    default_hasher{}(value, value_hash);
  }

  bytes32 hash() const {
    return sparse_merkle_leaf_node{account_key, value_hash}.hash();
  }

  friend bool operator==(const leaf_node<T>& a, const leaf_node<T>& b) {
    return std::tie(a.account_key, a.value_hash, a.value) == std::tie(b.account_key, b.value_hash, b.value);
  }

  bytes32 account_key;
  bytes32 value_hash;
  T value;
};

template<typename F, typename T>
void for_each_field(F&& f, leaf_node<T>& v) {
  f(v.account_key);
  f(v.value_hash);
  f(v.value);
}

template<typename F, typename T>
void for_each_field(F&& f, const leaf_node<T>& v) {
  f(v.account_key);
  f(v.value_hash);
  f(v.value);
}

enum class node_tag : uint8_t {
  null = 0,
  // internal_legacy = 1,
  leaf = 2,
  internal = 3,
};

template<typename T>
struct node {
  static node internal(const children& ch) {
    node n;
    n.data = internal_node(ch);
    return n;
  }

  static node leaf(const bytes32& account_key, const T& value) {
    node n;
    n.data = leaf_node(account_key, value);
    return n;
  }

  bool is_leaf() const {
    return std::holds_alternative<leaf_node<T>>(data);
  }

  jmt::node_type node_type() {
    if (std::holds_alternative<leaf_node<T>>(data)) {
      return jmt::leaf{};
    } else if (std::holds_alternative<internal_node>(data)) {
      return std::get<internal_node>(data).node_type();
    }
    check(false);
    __builtin_unreachable();
  }

  size_t leaf_count() {
    if (std::holds_alternative<null>(data)) {
      return 0;
    } else if (std::holds_alternative<leaf_node<T>>(data)) {
      return 1;
    } else if (std::holds_alternative<internal_node>(data)) {
      return std::get<internal_node>(data).leaf_count;
    }
  }

  std::vector<uint8_t> encode() const {
    std::vector<uint8_t> out;
    if (std::holds_alternative<null>(data)) {
      out.push_back(uint8_t(node_tag::null));
    } else if (std::holds_alternative<internal_node>(data)) {
      out.push_back(uint8_t(node_tag::internal));
      std::get<internal_node>(data).serialize(out);
      // TODO: metric
    } else if (std::holds_alternative<leaf_node<T>>(data)) {
      out.push_back(uint8_t(node_tag::leaf));
      auto bytes = codec::bcs::encode(std::get<leaf_node<T>>(data));
      out.insert(out.end(), bytes.begin(), bytes.end());
      // TODO: metric
    }
    return out;
  }

  bytes32 hash() const {
    return std::visit(overloaded{
                        [&](const null&) { return sparse_merkle_placeholder_hash; },
                        [&](const internal_node& n) { return n.hash(); },
                        [&](const leaf_node<T>& n) { return n.hash(); },
                      },
      data);
  }

  static result<node<T>> decode(std::span<uint8_t> val) {
    if (val.empty()) {
      return make_unexpected("missing tag due to empty input");
    }
    auto tag = val[0];
    auto n_tag = node_tag(tag);
    switch (n_tag) {
    case node_tag::null:
      return node<T>();
    case node_tag::internal:
      return node<T>{internal_node::deserialize({(const char*)val.data() + 1, val.size() - 1})};
    case node_tag::leaf:
      return node<T>{codec::bcs::decode<leaf_node<T>>({(const char*)val.data() + 1, val.size() - 1})};
    default:
      return make_unexpected(fmt::format("lead tag byte is unknown: {}", tag));
    }
  }

  std::variant<null, internal_node, leaf_node<T>> data;

  friend bool operator==(const node<T>& a, const node<T>& b) {
    if (a.data.index() != b.data.index())
      return false;
    switch (a.data.index()) {
    case 1:
      return std::get<1>(a.data) == std::get<1>(b.data);
    case 2:
      return std::get<2>(a.data) == std::get<2>(b.data);
    }
    return true;
  }
};

template<typename T>
using node_batch = std::map<node_key, node<T>>;

struct stale_node_index {
  std::string to_string() const {
    return fmt::format("{{version: {}, node_key:{}}}", stale_since_version, node_key.to_string());
  }

  version stale_since_version;
  jmt::node_key node_key;
};

inline bool operator==(const stale_node_index& a, const stale_node_index& b) {
  return std::tie(a.stale_since_version, a.node_key) == std::tie(b.stale_since_version, b.node_key);
}

inline bool operator<(const stale_node_index& a, const stale_node_index& b) {
  return std::tie(a.stale_since_version, a.node_key) < std::tie(b.stale_since_version, b.node_key);
}

using stale_node_index_batch = std::set<stale_node_index>;

struct node_stats {
  size_t new_nodes;
  size_t new_leaves;
  size_t stale_nodes;
  size_t stale_leaves;
};

inline bool operator==(const node_stats& a, const node_stats& b) {
  return std::tie(a.new_nodes, a.new_leaves, a.stale_nodes, a.stale_leaves) ==
    std::tie(b.new_nodes, b.new_leaves, b.stale_nodes, b.stale_leaves);
}

template<typename T>
struct tree_update_batch {
  jmt::node_batch<T> node_batch;
  jmt::stale_node_index_batch stale_node_index_batch;
  std::vector<jmt::node_stats> node_stats;
};

template<typename T>
inline bool operator==(const tree_update_batch<T>& a, const tree_update_batch<T>& b) {
  return std::tie(a.node_batch, a.stale_node_index_batch, a.node_stats) ==
    std::tie(b.node_batch, b.stale_node_index_batch, b.node_stats);
}

template<typename T>
struct tree_reader {
  using value_type = T;

  auto get_node(const jmt::node_key& node_key) -> result<jmt::node<T>> {
    auto node = get_node_option(node_key);
    if (node && *node)
      return **node;
    return make_unexpected(fmt::format("missing node at key: {}", node_key.to_string()));
  }
  virtual auto get_node_option(const jmt::node_key& node_key) -> result<std::optional<node<T>>> = 0;
  virtual auto get_rightmost_leaf() -> result<std::optional<std::pair<jmt::node_key, leaf_node<T>>>> = 0;
};

template<typename T>
struct tree_writer {
  virtual auto write_node_batch(const jmt::node_batch<T>& node_batch) -> result<void> = 0;
};

} // namespace noir::jmt

namespace std {

template<>
struct hash<noir::jmt::node_key> {
  std::size_t operator()(const noir::jmt::node_key& key) const noexcept {
    noir::crypto::xxh64 hash;
    auto nhash = std::hash<noir::jmt::nibble_path>()(key.nibble_path);
    hash.init().update({(char*)&key.version, 8}).update({(char*)&nhash, sizeof(nhash)});
    return hash.final();
  }
};

} // namespace std

NOIR_REFLECT(noir::jmt::internal_node, children, leaf_count);
