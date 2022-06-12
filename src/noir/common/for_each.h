// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/foreachable.h>
#include <noir/common/refl.h>
#include <boost/pfr.hpp>

namespace noir {

template<typename T>
requires (std::is_class_v<T> && std::is_aggregate_v<T>)
struct IsForeachable<T> : std::true_type {};

template<typename F, Foreachable T>
void for_each_field(F&& f, T& v) {
  if constexpr (refl::has_refl_v<T>) {
    refl::for_each_field(
      [f{std::forward<F>(f)}](const auto& desc, auto& value) {
        f(value);
        return true;
      },
      v);
  } else {
    boost::pfr::for_each_field(v, std::forward<F>(f));
  }
}

template<typename F, Foreachable T>
void for_each_field(F&& f, const T& v) {
  if constexpr (refl::has_refl_v<T>) {
    refl::for_each_field(
      [f{std::forward<F>(f)}](const auto& desc, const auto& value) {
        f(value);
        return true;
      },
      v);
  } else {
    boost::pfr::for_each_field(v, std::forward<F>(f));
  }
}

} // namespace noir
