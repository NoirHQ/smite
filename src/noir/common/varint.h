// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/result.h>
#include <compare>

namespace noir {

template<typename T>
struct Varint {
  constexpr Varint(int v = 0): value(v) {}

  template<typename U>
  constexpr Varint(U v): value(v) {}

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
constexpr auto operator<=>(const Varint<T>& a, const U& b) {
  return a.value <=> b;
}

template<typename T, typename U>
constexpr auto operator<=>(const U& a, const Varint<T>& b) {
  return a <=> b.value;
}

template<typename T, typename U>
constexpr auto operator<=>(const Varint<T>& a, const Varint<U>& b) {
  return a.value <=> b.value;
}

using Varint32 = Varint<int32_t>;
using Varint64 = Varint<int64_t>;
using Varuint32 = Varint<uint32_t>;
using Varuint64 = Varint<uint64_t>;

// TODO: error handling for ds.put()
template<typename Stream, typename T>
auto write_uleb128(Stream& ds, const Varint<T>& v) -> Result<size_t> {
  std::make_unsigned_t<T> val = v.value;
  auto max_len = (sizeof(T) * 8 + 6) / 7;
  auto i = 0;
  for (; i < max_len; ++i) {
    uint8_t low_bits = val & 0x7f;
    val >>= 7;
    uint8_t more = val ? 0x80 : 0;
    ds.put(uint8_t(low_bits | more));
    if (!more)
      break;
  }
  return i + 1;
}

// TODO: error handling for ds.get()
template<typename Stream, typename T>
auto read_uleb128(Stream& ds, Varint<T>& v) -> Result<size_t> {
  std::make_unsigned_t<T> val = 0;
  auto max_len = (sizeof(T) * 8 + 6) / 7;
  auto i = 0;
  for (; i < max_len - 1; ++i) {
    auto res = ds.get();
    if (!res) {
      return res.error();
    }
    uint8_t c = res.value();
    val |= T(c & 0x7f) << (i * 7);
    if (!(c & 0x80)) {
      if (!c && (i > 0)) {
        return Error("ULEB128: most significant byte must not be 0");
      }
      v = val;
      return i + 1;
    }
  }
  auto res = ds.get();
  if (!res) {
    return res.error();
  }
  uint8_t c = res.value();
  // FIXME: this check makes parsing messages from go-implmented abci server failed.
  // CHECK(!(c >> (sizeof(T) * 8 - (max_len - 1) * 7)), "ULEB128: unused bits must be cleared");
  val |= T(c) << (max_len - 1) * 7;
  v = val;
  return max_len;
}

template<typename Stream, typename T>
auto write_zigzag(Stream& ds, const Varint<T>& v) -> Result<size_t> {
  auto val = v.value;
  val = (val >> (sizeof(T) * 8 - 1)) ^ (val << 1);
  return write_uleb128(ds, Varint<decltype(val)>(val));
}

template<typename Stream, typename T>
auto read_zigzag(Stream& ds, Varint<T>& v) -> Result<size_t> {
  Varint<std::make_unsigned_t<T>> val = 0;
  auto ret = read_uleb128(ds, val);
  v.value = (val >> 1) ^ (-(val & 1));
  return ret;
}

template<typename Stream, typename T>
auto read_uleb128_async(Stream& ds, Varint<T>& v) -> boost::asio::awaitable<Result<size_t>> {
  std::make_unsigned_t<T> val = 0;
  auto max_len = (sizeof(T) * 8 + 6) / 7;
  auto i = 0;
  for (; i < max_len - 1; ++i) {
    uint8_t c = 0;
    if (auto ok = co_await ds.read(&c, 1); !ok) {
      co_return ok.error();
    }
    val |= T(c & 0x7f) << (i * 7);
    if (!(c & 0x80)) {
      if (!c && (i > 0)) {
        co_return Error("ULEB128: most significant byte must not be 0");
      }
      v = val;
      co_return i + 1;
    }
  }
  uint8_t c = 0;
  if (auto ok = co_await ds.read(&c, 1); !ok) {
    co_return ok.error();
  }
  // FIXME: this check makes parsing messages from go-implmented abci server failed.
  // CHECK(!(c >> (sizeof(T) * 8 - (max_len - 1) * 7)), "ULEB128: unused bits must be cleared");
  val |= T(c) << (max_len - 1) * 7;
  v = val;
  co_return max_len;
}

template<typename Stream, typename T>
auto read_zigzag_async(Stream& ds, Varint<T>& v) -> boost::asio::awaitable<Result<size_t>> {
  Varint<std::make_unsigned_t<T>> val = 0;
  auto size = co_await read_uleb128_async(ds, val);
  v.value = (val >> 1) ^ (-(val & 1));
  co_return size;
}

} // namespace noir
