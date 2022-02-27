// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <stdexcept>
#include <string_view>
#include <fmt/format.h>

namespace noir {

/// \brief throws when the given condition is unmet
/// \tparam Error type of exception to be thrown
/// \param pred predicate function
/// \param msg message passed to the exception
/// \ingroup common
template<typename Error = std::runtime_error>
inline void check(bool pred, std::string_view msg) {
  if (!pred) {
    throw Error(msg.data());
  }
}

/// \brief throws when the given condition is unmet
/// \tparam Error type of exception to be thrown
/// \param pred predicate function
/// \param msg message passed to the exception
/// \ingroup common
template<typename Error = std::runtime_error>
inline void check(bool pred, const char* msg = "") {
  if (!pred) {
    throw Error(msg);
  }
}

/// \brief throws when the given condition is unmet
/// \tparam Error type of exception to be thrown
/// \tparam Ts arguments for the formatted message
/// \param format_str string for formatting
/// \param args variable arguments for formatted message
/// \ingroup common
template<typename Error = std::runtime_error, typename... Ts>
inline void check(bool pred, const char* format_str, Ts... args) {
  if (!pred) {
    throw Error(fmt::format(format_str, args...));
  }
}

} // namespace noir
