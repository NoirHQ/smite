// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/expected.h>
#include <string>

namespace noir {

using error = std::string;

/// \brief [rust] analogous Result<T, E> type
/// \ingroup common
template<typename T, typename E = error>
using result = expected<T, E>;

} // namespace noir

/// \brief [rust] analogous to ? operator that returns early when error occurs
/// \ingroup common
#define return_if_error(expr) \
  do { \
    auto _ = expr; \
    if (!_) { \
      return make_unexpected(_.error()); \
    } \
  } while (0)

/// \brief [rust] analogous to ensure! macro of anyhow crate
/// \ingroup common
#define ensure(pred, ...) \
  do { \
    if (!(pred)) { \
      return make_unexpected(fmt::format(__VA_ARGS__)); \
    } \
  } while (0)

/// \brief [rust] analogous to bail! macro of anyhow crate
/// \ingroup common
#define bail(...) return make_unexpected(fmt::format(__VA_ARGS__))
