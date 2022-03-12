// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/expected.h>
#include <optional>
#include <string>

namespace noir {

using error = std::string;

/// \brief [rust] analogous Result<T, E> type
/// \ingroup common
template<typename T, typename E = error>
using result = expected<T, E>;

template<typename T, typename... Ts>
auto expect(std::optional<T>&& v, Ts... msg) {
  check(v.has_value(), msg...);
  return v.value();
}

} // namespace noir

/// \brief [rust] analogous to ? operator that returns early when error occurs
/// \ingroup common
#define noir_ok(expr) \
  ({ \
    auto _ = expr; \
    if (!_) { \
      return noir::make_unexpected(_.error()); \
    } \
    _.value(); \
  })

/// \brief [rust] analogous to ensure! macro of anyhow crate
/// \ingroup common
#define noir_ensure(pred, ...) \
  do { \
    if (!(pred)) { \
      return noir::make_unexpected(fmt::format(__VA_ARGS__)); \
    } \
  } while (0)

/// \brief [rust] analogous to bail! macro of anyhow crate
/// \ingroup common
#define noir_bail(...) return noir::make_unexpected(fmt::format(__VA_ARGS__))
