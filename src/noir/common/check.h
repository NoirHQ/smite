// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

namespace noir {

inline void check(bool pred, std::string_view msg) {
  if (!pred) {
    throw std::runtime_error(msg.data());
  }
}

inline void check(bool pred, const char* msg = "") {
  if (!pred) {
    throw std::runtime_error(msg);
  }
}

inline void check(bool pred, const std::string& msg) {
  if (!pred) {
    throw std::runtime_error(msg);
  }
}

inline void check(bool pred, std::string&& msg) {
  if (!pred) {
    throw std::runtime_error(msg);
  }
}

} // namespace noir
