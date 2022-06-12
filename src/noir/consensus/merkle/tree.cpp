// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/merkle/tree.h>
#include <functional>

namespace noir::consensus::merkle {

using hash_func = std::function<Bytes(std::span<const unsigned char>)>;
hash_func hash = [](std::span<const unsigned char> in) {
  return crypto::Sha256()(in); ///< use Sha256 for now; may use a different algorithm in the future
};

const char leaf_prefix = '\x00';
const char inner_prefix = '\x01';

Bytes get_empty_hash() {
  return hash({});
}

Bytes leaf_hash_opt(const Bytes& leaf) {
  Bytes buff;
  buff.raw().reserve(leaf.size() + 1);
  buff.raw().push_back(leaf_prefix);
  buff.raw().insert(buff.end(), leaf.begin(), leaf.end());
  return hash(buff);
}

Bytes inner_hash_opt(const Bytes& left, const Bytes& right) {
  Bytes buff;
  buff.raw().reserve(left.size() + right.size() + 1);
  buff.raw().push_back(inner_prefix);
  buff.raw().insert(buff.end(), left.begin(), left.end());
  buff.raw().insert(buff.end(), right.begin(), right.end());
  return hash(buff);
}

size_t get_split_point(size_t length) {
  check(length > 0, "try to split a merkle tree with size < 1");
  auto bit_len = std::bit_width(length);
  size_t k = 1 << (bit_len - 1);
  if (k == length)
    k >>= 1;
  return k;
}

Bytes hash_from_bytes_list(const bytes_list& list) {
  switch (list.size()) {
  case 0:
    return get_empty_hash();
  case 1:
    return leaf_hash_opt(list[0]);
  }
  auto k = get_split_point(list.size());
  auto left = hash_from_bytes_list({list.begin(), list.begin() + k});
  auto right = hash_from_bytes_list({list.begin() + k, list.end()});
  return inner_hash_opt(left, right);
}

Bytes compute_hash_from_aunts(int64_t index, int64_t total, Bytes leaf_hash, bytes_list inner_hashes) {
  if (index >= total || index < 0 || total <= 0)
    return {};
  switch (total) {
  case 0:
    check(false, "cannot call compute_hash_from_aunts with 0 total");
  case 1:
    if (!inner_hashes.empty())
      return {};
    return leaf_hash;
  }
  if (inner_hashes.empty())
    return {};
  auto num_left = get_split_point(total);
  if (index < num_left) {
    auto left_hash =
      compute_hash_from_aunts(index, num_left, leaf_hash, {inner_hashes.begin(), inner_hashes.end() - 1});
    if (left_hash.empty())
      return {};
    return inner_hash_opt(left_hash, inner_hashes[inner_hashes.size() - 1]);
  }
  auto right_hash = compute_hash_from_aunts(
    index - num_left, total - num_left, leaf_hash, {inner_hashes.begin(), inner_hashes.end() - 1});
  if (right_hash.empty())
    return {};
  return inner_hash_opt(inner_hashes[inner_hashes.size() - 1], right_hash);
}

} // namespace noir::consensus::merkle
