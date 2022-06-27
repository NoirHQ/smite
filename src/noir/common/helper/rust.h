// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <optional>
#include <string>

namespace noir {

template<typename T, typename... Ts>
auto expect(std::optional<T>&& v, Ts... msg) {
  if (!v) {
    throw std::runtime_error(msg...);
  }
  return v.value();
}

} // namespace noir

/// \brief [rust] analogous to ? operator that returns early when error occurs
/// \ingroup common
#define noir_ok(expr) \
  ({ \
    auto _ = expr; \
    if (!_) { \
      return _.error(); \
    } \
    _.value(); \
  })

/// \brief [rust] analogous to ensure! macro of anyhow crate
/// \ingroup common
#define noir_ensure(pred, ...) \
  do { \
    if (!(pred)) { \
      return Error::format(__VA_ARGS__); \
    } \
  } while (0)

/// \brief [rust] analogous to bail! macro of anyhow crate
/// \ingroup common
#define noir_bail(...) return Error::format(__VA_ARGS__)
