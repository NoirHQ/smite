// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/check.h>
#include <noir/common/expected.h>
#include <noir/common/for_each.h>
#include <noir/common/pow.h>
#include <noir/common/types.h>
#include <boost/pfr.hpp>
#include <optional>
#include <variant>

NOIR_CODEC(scale) {

// Fixed-width integers
// Boolean
template<typename Stream, typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  ds.write((const char*)&v, sizeof(v));
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  ds.read({(char*)&v, sizeof(v)});
  return ds;
}

// Compact/general integers
template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const unsigned_int& v) {
  using noir::pown;
  if (v < pown(2u, 6)) {
    ds.put(v.value << 2);
  } else if (v < pown(2u, 14)) {
    uint16_t tmp = v.value << 2 | 0b01;
    ds.write((char*)&tmp, 2);
  } else if (v < pown(2u, 30)) {
    uint32_t tmp = v.value << 2 | 0b10;
    ds.write((char*)&tmp, 4);
  } else {
    // TODO
    check(false, "not implemented");
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, unsigned_int& v) {
  char tmp = ds.peek();
  switch (tmp & 0b11) {
  case 0b00: {
    uint8_t val = 0;
    ds >> val;
    val >>= 2;
    v = val;
  } break;
  case 0b01: {
    uint16_t val = 0;
    ds >> val;
    val >>= 2;
    v = val;
  } break;
  case 0b10: {
    uint32_t val = 0;
    ds >> val;
    val >>= 2;
    v = val;
  } break;
  case 0b11:
    // TODO
    check(false, "not implemented");
    break;
  }
  return ds;
}

// Options (bool)
template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::optional<bool>& v) {
  char val = v.has_value();
  if (val) {
    val += !*v;
  }
  ds << val;
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::optional<bool>& v) {
  char has_value = 0;
  ds >> has_value;
  if (has_value) {
    v = !(has_value - 1);
  } else {
    v.reset();
  }
  return ds;
}

// Options (except for bool)
template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::optional<T>& v) {
  char has_value = v.has_value();
  ds << has_value;
  if (has_value)
    ds << *v;
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::optional<T>& v) {
  char has_value = 0;
  ds >> has_value;
  if (has_value) {
    T val;
    ds >> val;
    v = val;
  } else {
    v.reset();
  }
  return ds;
}

// Results
template<typename Stream, typename T, typename E>
datastream<Stream>& operator<<(datastream<Stream>& ds, const noir::expected<T, E>& v) {
  char is_unexpected = !v;
  ds << is_unexpected;
  if (v) {
    ds << *v;
  } else {
    ds << v.error();
  }
  return ds;
}

template<typename Stream, typename T, typename E>
datastream<Stream>& operator>>(datastream<Stream>& ds, noir::expected<T, E>& v) {
  char is_unexpected = 0;
  ds >> is_unexpected;
  if (!is_unexpected) {
    T val;
    ds >> val;
    v = val;
  } else {
    E err;
    ds >> err;
    v = noir::make_unexpected(err);
  }
  return ds;
}

// Vectors (lists, series, sets)
// TODO: Add other containers
template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::vector<T>& v) {
  ds << unsigned_int(v.size());
  for (const auto& i : v) {
    ds << i;
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::vector<T>& v) {
  unsigned_int size;
  ds >> size;
  v.resize(size);
  for (auto& i : v) {
    ds >> i;
  }
  return ds;
}

template<typename Stream, typename T, size_t N>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T (&v)[N]) {
  std::span s(v);
  for (const auto& i : s) {
    ds << i;
  }
  return ds;
}

template<typename Stream, typename T, size_t N>
datastream<Stream>& operator>>(datastream<Stream>& ds, T (&v)[N]) {
  std::span s(v);
  for (auto& i : s) {
    ds >> i;
  }
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::is_same_v<std::remove_cv_t<T>, char>, bool> = true>
datastream<Stream>& operator<<(datastream<Stream>& ds, std::span<T> v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::is_same_v<std::remove_cv_t<T>, char>, bool> = true>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::span<T> v) {
  ds.read(v.data(), v.size());
  return ds;
}

// Strings
template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string& v) {
  ds << unsigned_int(v.size());
  if (v.size()) {
    ds.write(v.data(), v.size());
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::string& v) {
  std::vector<char> tmp;
  ds >> tmp;
  if (tmp.size()) {
    v = std::string(tmp.data(), tmp.data() + tmp.size());
  } else {
    v = {};
  }
  return ds;
}

// Tuples
template<typename Stream, typename... Ts>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::tuple<Ts...>& v) {
  std::apply([&](const auto&... val) { ((ds << val), ...); }, v);
  return ds;
}

template<typename Stream, typename... Ts>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::tuple<Ts...>& v) {
  std::apply([&](auto&... val) { ((ds >> val), ...); }, v);
  return ds;
}

// Data Structures
template<typename Stream, typename T, std::enable_if_t<is_foreachable_v<T>, bool> = true>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  for_each_field(v, [&](const auto& val) { ds << val; });
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<is_foreachable_v<T>, bool> = true>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  auto f = [&](auto& val) { ds >> val; };
  for_each_field(v, [&](auto& val) { ds >> val; });
  return ds;
}

// Enumerations (tagged-unions)
template<typename Stream, typename... Ts>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::variant<Ts...>& v) {
  check(v.index() <= 0xff, "no more than 256 variants are supported");
  char index = v.index();
  ds << index;
  std::visit([&](auto& val) { ds << val; }, v);
  return ds;
}

namespace detail {
  template<size_t I, typename Stream, typename... Ts>
  void decode(datastream<Stream>& ds, std::variant<Ts...>& v, int i) {
    if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
      if (i == I) {
        std::variant_alternative_t<I, std::variant<Ts...>> tmp;
        ds >> tmp;
        v.template emplace<I>(std::move(tmp));
      } else {
        decode<I + 1>(ds, v, i);
      }
    } else {
      check(false, "invalid variant index");
    }
  }
} // namespace detail

template<typename Stream, typename... Ts>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::variant<Ts...>& v) {
  char index = 0;
  ds >> index;
  detail::decode<0>(ds, v, index);
  return ds;
}

} // NOIR_CODEC(scale)
