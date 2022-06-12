// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <string>

namespace noir {

namespace detail {
  template<typename T>
  std::string to_str(T&& v) {
    using namespace std;
    return to_string(std::forward<T>(v));
  }
} // namespace detail

template<typename T>
std::string to_string(T&& v) {
  constexpr bool has_to_string = requires(T v) {
    v.to_string();
  };
  // clang-format off
  if constexpr (has_to_string) {
    return v.to_string();
  } else {
    return detail::to_str(std::forward<T>(v));
  }
  // clang-format on
}

} // namespace noir
