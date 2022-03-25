// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/common/type_traits.h>
#include <cstddef>

namespace noir::codec::protobuf {

template<size_t I, typename T>
size_t _unset_fields_count(const T& v, size_t count) {
  if constexpr (I < refl::fields_count_v<T>) {
    if constexpr (is_optional_v<refl::field<I, T>>) {
      return _unset_fields_count<I + 1, T>(v, count + !refl::get<I>(v).has_value());
    }
  }
  return count;
}

template<typename T>
size_t unset_fields_count(const T& v) {
  return _unset_fields_count<0, T>(v, 0);
}

} // namespace noir::codec::protobuf