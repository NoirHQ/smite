// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/expected.h>
#include <noir/common/types/hash.h>
#include <noir/common/types/bytes.h>
#include <string>
#include <unordered_map>

namespace noir::jmt {

using error = std::string;

template<typename T, typename E = error>
using result = expected<T, E>;

using version = uint64_t;

size_t common_prefix_bits_len(const bytes32& a, const bytes32& b);

extern bytes32 sparse_merkle_placeholder_hash;

template<typename K, typename V>
using unordered_map = std::unordered_map<K, V, noir::hash<K>>;

} // namespace noir::jmt

#define return_if_error(expr) \
  do { \
    auto _ = expr; \
    if (!_) { \
      return make_unexpected(_.error()); \
    } \
  } while (0)

#define ensure(pred, ...) \
  do { \
    if (!(pred)) { \
      return make_unexpected(fmt::format(__VA_ARGS__)); \
    } \
  } while (0)

#define bail(...) return make_unexpected(fmt::format(__VA_ARGS__))
