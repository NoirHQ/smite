// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
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

  constexpr auto& operator=(T v) {
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

template<typename T, typename U>
constexpr auto operator<=>(const varint<T>& a, const varint<U>& b) {
  return a.value <=> b.value;
}

using varint32 = varint<int32_t>;
using varint64 = varint<int64_t>;
using varuint32 = varint<uint32_t>;
using varuint64 = varint<uint64_t>;

// FIXME: remove later
using signed_int = varint32;
using unsigned_int = varuint32;

template<typename DataStream, typename T>
size_t write_uleb128(DataStream& ds, const varint<T>& v) {
  std::make_unsigned_t<T> val = v.value;
  auto max_len = (sizeof(T) * 8 + 6) / 7;
  auto i = 0;
  for (; i < max_len; ++i) {
    uint8_t low_bits = val & 0x7f;
    val >>= 7;
    uint8_t more = val ? 0x80 : 0;
    ds.put(low_bits | more);
    if (!more)
      break;
  }
  return i + 1;
}

template<typename DataStream, typename T>
size_t read_uleb128(DataStream& ds, varint<T>& v) {
  std::make_unsigned_t<T> val = 0;
  auto max_len = (sizeof(T) * 8 + 6) / 7;
  auto i = 0;
  for (; i < max_len - 1; ++i) {
    uint8_t c = ds.get();
    val |= T(c & 0x7f) << (i * 7);
    if (!(c & 0x80)) {
      check(c || !i, "uleb128: most significant byte must not be 0");
      v = val;
      return i + 1;
    }
  }
  uint8_t c = ds.get();
  check(!(c >> (sizeof(T) * 8 - (max_len - 1) * 7)), "uleb128: unused bits must be cleared");
  val |= T(c) << (max_len - 1) * 7;
  v = val;
  return max_len;
}

template<typename DataStream, typename T>
size_t write_zigzag(DataStream& ds, const varint<T>& v) {
  auto val = v.value;
  val = (val >> (sizeof(T) * 8 - 1)) ^ (val << 1);
  return write_uleb128(ds, varint<decltype(val)>(val));
}

template<typename DataStream, typename T>
size_t read_zigzag(DataStream& ds, varint<T>& v) {
  std::make_unsigned_t<T> val;
  auto ret = read_uleb128(ds, variant<decltype(val)>(val));
  v.value = (val >> 1) ^ (-(val & 1));
  return ret;
}

} // namespace noir
