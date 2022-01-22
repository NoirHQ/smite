// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/check.h>
#include <noir/common/for_each.h>
#include <span>

NOIR_CODEC(rlp) {

namespace detail {
  template<typename Stream, typename T>
  void encode_bytes(datastream<Stream>& ds, const T& v, unsigned char mod) {
    auto s = std::span((const char*)&v, sizeof(T));
    auto nonzero = std::find_if(s.rbegin(), s.rend(), [](const auto& c) {
      if (c != 0)
        return true;
      return false;
    });
    auto trimmed = std::distance(nonzero, s.rend());
    // encode a value less than 128 (0x80) as single byte
    if (trimmed == 1 && ((v & 0xff) < 0x80u)) {
      ds.put(v & 0xff);
      return;
    }
    ds.put(trimmed + mod);
    std::for_each(s.rend() - trimmed, s.rend(), [&](const auto& c) { ds.write(&c, 1); });
  }

  template<typename Stream, typename T>
  void encode_prefix(datastream<Stream>& ds, const T& v, unsigned char mod) {
    // encode a length prefix less than 55
    if (v <= 55) {
      ds.put(v + mod);
      return;
    }
    auto s = std::span((const char*)&v, sizeof(T));
    auto nonzero = std::find_if(s.rbegin(), s.rend(), [](const auto& c) {
      if (c != 0)
        return true;
      return false;
    });
    auto trimmed = std::distance(nonzero, s.rend());
    // adjust the modifier to encode a length prefix greater than 55
    // e.g. bytes (0x80 => 0xb7), lists (0xc0 => 0xf7)
    mod += 55;
    ds.put(trimmed + mod);
    std::for_each(s.rend() - trimmed, s.rend(), [&](const auto& c) { ds.write(&c, 1); });
  }

  template<typename Stream>
  uint64_t decode_bytes(datastream<Stream>& ds, unsigned char prefix, unsigned char mod) {
    // decode a value less than 128 (0x80)
    if (prefix < 0x80) {
      return prefix;
    }
    auto size = prefix - mod;
    auto v = 0ull;
    auto s = std::span((char*)&v, sizeof(v));
    std::for_each(s.rend() - size, s.rend(), [&](auto& c) { ds.get(c); });
    return v;
  }

  template<typename Stream>
  uint64_t decode_prefix(datastream<Stream>& ds, unsigned char prefix, unsigned char mod) {
    auto size = prefix - mod;
    if (size <= 55) {
      // decode a length prefix less than 55
      return size;
    } else {
      // decode a length prefix greater than 55 (need to subtract 55 from modifier)
      size -= 55;
    }
    auto v = 0ull;
    auto s = std::span((char*)&v, sizeof(v));
    std::for_each(s.rend() - size, s.rend(), [&](auto& c) { ds.get(c); });
    return v;
  }
} // namespace detail

// integers
template<typename Stream, typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  static_assert(sizeof(T) <= 55);
  detail::encode_bytes(ds, v, 0x80);
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  auto prefix = static_cast<unsigned char>(ds.get());
  if (prefix < 0xb8) {
    check(prefix <= 0x80 + sizeof(T), "not sufficient output size");
    v = detail::decode_bytes(ds, prefix, 0x80);
  } else if (prefix < 0xc0) {
    // TODO
    check(false, "not implemented");
  } else {
    check(false, "not matched prefix type");
  }
  return ds;
}

// list
template<typename Stream, typename T, size_t N>
datastream<Stream>& operator<<(datastream<Stream>& ds, std::span<T, N> v) {
  auto size = 0ull;
  for (const auto& val : v) {
    size += encode_size(val);
  }
  detail::encode_prefix(ds, size, 0xc0);
  for (const auto& val : v) {
    ds << val;
  }
  return ds;
}

template<typename Stream, typename T, size_t N>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::span<T, N> v) {
  auto prefix = static_cast<unsigned char>(ds.get());
  check(prefix >= 0xc0, "not matched prefix type");
  auto size = detail::decode_prefix(ds, prefix, 0xc0);
  auto i = 0;
  while (size > 0) {
    auto start = ds.tellp();
    ds >> v[i++];
    size -= (ds.tellp() - start);
  }
  return ds;
}

template<typename Stream, typename T, size_t N>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T (&v)[N]) {
  ds << std::span(v);
  return ds;
}

template<typename Stream, typename T, size_t N>
datastream<Stream>& operator>>(datastream<Stream>& ds, T (&v)[N]) {
  ds >> std::span(v);
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::vector<T>& v) {
  ds << std::span(v);
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::vector<T>& v) {
  auto prefix = static_cast<unsigned char>(ds.get());
  check(prefix >= 0xc0, "not matched prefix type");
  auto size = detail::decode_prefix(ds, prefix, 0xc0);
  while (size > 0) {
    auto start = ds.tellp();
    T tmp;
    ds >> tmp;
    v.push_back(tmp);
    size -= (ds.tellp() - start);
  }
  return ds;
}

// strings
template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string& v) {
  auto size = v.size();
  if (size == 1 && v[0] < 0x80u) {
    ds.put(v[0]);
  } else {
    detail::encode_prefix(ds, size, 0x80);
    ds.write(v.data(), size);
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::string& v) {
  auto prefix = static_cast<unsigned char>(ds.get());
  if (prefix < 0x80) {
    v.resize(1);
    v[0] = prefix;
  } else if (prefix < 0xc0) {
    auto size = detail::decode_prefix(ds, prefix, 0x80);
    v.resize(size);
    ds.read(v);
  } else {
    check(false, "not matched prefix type");
  }
  return ds;
}

// structs
template<typename Stream, typename T, std::enable_if_t<std::is_class_v<T>, bool> = true>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  auto size = 0ull;
  for_each_field(v, [&](const auto& val) { size += encode_size(val); });
  detail::encode_prefix(ds, size, 0xc0);
  for_each_field(v, [&](const auto& val) { ds << val; });
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::is_class_v<T>, bool> = true>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  auto prefix = static_cast<unsigned char>(ds.get());
  check(prefix >= 0xc0, "not matched prefix type");
  auto size = detail::decode_prefix(ds, prefix, 0xc0);
  for_each_field(v, [&](auto& val) {
    auto start = ds.tellp();
    ds >> val;
    size -= (ds.tellp() - start);
    check(size >= 0, "insufficient bytes provided");
  });
  return ds;
}

} // NOIR_CODEC(rlp)
