// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <compare>

namespace noir {

template<typename T>
struct varint {
  constexpr varint(int v = 0): value(v) {}

  template<typename U>
  constexpr varint(U v): value(v) {}

  constexpr operator T() const {
    return value;
  }

  constexpr auto& operator=(int v) {
    value = v;
    return *this;
  }

  T value;
};

template<typename T, typename U>
constexpr auto operator<=>(const varint<T>& a, const U& b) {
  return a.value <=> b;
}

template<typename T, typename U>
constexpr auto operator<=>(const U& a, const varint<T>& b) {
  return a <=> b.value;
}

template<typename T>
constexpr auto operator<=>(const varint<T>& a, const varint<T>& b) {
  return a.value <=> b.value;
}

using varint32 = varint<int32_t>;
using varint64 = varint<int64_t>;
using varuint32 = varint<uint32_t>;
using varuint64 = varint<uint64_t>;

// FIXME: remove later
using signed_int = varint32;
using unsigned_int = varuint32;

} // namespace noir
