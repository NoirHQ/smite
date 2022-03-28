// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/for_each.h>
#include <noir/common/types.h>
#include <boost/pfr.hpp>
#include <map>
#include <memory>
#include <optional>
#include <variant>

NOIR_CODEC(bcs) {

// Booleans and Integers
template<typename Stream, integral T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  ds.write((const char*)&v, sizeof(v));
  return ds;
}

template<typename Stream, integral T>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  ds.read({(char*)&v, sizeof(v)});
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const uint256_t& v) {
  uint64_t data[4] = {0};
  boost::multiprecision::export_bits(v, std::begin(data), 64, false);
  ds << std::span((const char*)data, 32);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, uint256_t& v) {
  uint64_t data[4] = {0};
  ds >> std::span((char*)data, 32);
  boost::multiprecision::import_bits(v, std::begin(data), std::end(data), 64, false);
  return ds;
}

template<typename Stream, enumeration E>
datastream<Stream>& operator<<(datastream<Stream>& ds, const E& v) {
  ds << static_cast<const std::underlying_type_t<E>>(v);
  return ds;
}

template<typename Stream, enumeration E>
datastream<Stream>& operator>>(datastream<Stream>& ds, E& v) {
  std::underlying_type_t<E> tmp;
  ds >> tmp;
  v = static_cast<E>(tmp);
  return ds;
}

// ULEB128-Encoded Integers
template<typename Stream, unsigned_integral T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const varint<T>& v) {
  write_uleb128(ds, v);
  return ds;
}

template<typename Stream, unsigned_integral T>
datastream<Stream>& operator>>(datastream<Stream>& ds, varint<T>& v) {
  read_uleb128(ds, v);
  return ds;
}

// Optional Data
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

// Fixed and Variable Length Sequences
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

template<typename Stream, Byte T, size_t N>
datastream<Stream>& operator<<(datastream<Stream>& ds, std::span<T, N> v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<typename Stream, Byte T, size_t N>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::span<T, N> v) {
  ds.read(v.data(), v.size());
  return ds;
}

template<typename Stream, typename K, typename V>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::map<K, V>& v) {
  ds << varuint32(v.size());
  for (const auto& i : v) {
    ds << i.first << i.second;
  }
  return ds;
}

template<typename Stream, typename K, typename V>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::map<K, V>& v) {
  v.clear();
  varuint32 size = 0;
  ds >> size;
  for (auto i = 0; i < size.value; ++i) {
    K key;
    V value;
    ds >> key >> value;
    v.emplace(std::move(key), std::move(value));
  }
  return ds;
}

// Strings (utf-8 support?)
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
  unsigned_int size;
  ds >> size;
  v.resize(size);
  if (size) {
    ds.read(v.data(), v.size());
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

// Structures
template<typename Stream, foreachable T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  for_each_field([&](const auto& val) { ds << val; }, v);
  return ds;
}

template<typename Stream, foreachable T>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  for_each_field([&](auto& val) { ds >> val; }, v);
  return ds;
}

// Enumerations (tagged-unions)
template<typename Stream, typename... Ts>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::variant<Ts...>& v) {
  ds << unsigned_int(v.index());
  std::visit([&](auto& val) { ds << val; }, v);
  return ds;
}

namespace detail {
  template<size_t I, typename Stream, typename... Ts>
  void decode(datastream<Stream>& ds, std::variant<Ts...>& v, unsigned_int i) {
    if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
      if (i.value == I) {
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
  unsigned_int index = 0;
  ds >> index;
  detail::decode<0>(ds, v, index);
  return ds;
}

// Shared pointer
template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::shared_ptr<T>& v) {
  ds << bool(!!v);
  if (!!v)
    ds << *v;
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::shared_ptr<T>& v) {
  bool b;
  ds >> b;
  if (b) {
    v = std::make_shared<T>();
    ds >> *v;
  }
  return ds;
}

} // NOIR_CODEC(bcs)
