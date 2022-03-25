// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <boost/pfr.hpp>

namespace noir {

template<typename T, typename = void>
struct is_foreachable : std::false_type {};

template<typename T>
struct is_foreachable<T, std::enable_if_t<std::is_class_v<T>>> : std::true_type {};

template<typename T>
constexpr bool is_foreachable_v = is_foreachable<T>::value;

template<typename T>
concept foreachable = is_foreachable_v<T>;

template<typename F, typename T>
void for_each_field(F&& f, T& v) {
  if constexpr (refl::has_refl_v<T>) {
    refl::for_each_field(
      [&](const auto& desc, auto& value) {
        f(value);
        return true;
      },
      v);
  } else {
    boost::pfr::for_each_field(v, f);
  }
}

template<typename F, typename T>
void for_each_field(F&& f, const T& v) {
  if constexpr (refl::has_refl_v<T>) {
    refl::for_each_field(
      [&](const auto& desc, const auto& value) {
        f(value);
        return true;
      },
      v);
  } else {
    boost::pfr::for_each_field(v, f);
  }
}

} // namespace noir
