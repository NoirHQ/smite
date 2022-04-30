// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <string>

namespace noir {

template<typename T>
std::string to_string(const T& v) {
  constexpr bool has_to_string = requires(const T& t) {
    t.to_string();
  };
  // clang-format off
  if constexpr (has_to_string) {
    return v.to_string();
  } else {
    return std::to_string(v);
  }
  // clang-format on
}

} // namespace noir
