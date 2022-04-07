// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <cmath>
#include <iostream>
#include <span>
#include <vector>

namespace noir::codec::orderedcode {

template<typename T>
class datastream : public basic_datastream<T> {
public:
  using basic_datastream<T>::basic_datastream;
};

template<typename T>
constexpr size_t encode_size(const T& v) {
  datastream<size_t> ds;
  ds << v;
  return ds.tellp();
}

template<typename Stream, typename T>
constexpr void encode_size(datastream<Stream>& ds, const T& v) {
  ds << v;
}

template<typename Stream, typename T, typename... Ts>
constexpr void encode_size(datastream<Stream>& ds, const T& v, const Ts&... vs) {
  ds << v;
  encode_size(ds, vs...);
}

template<typename... Ts>
constexpr size_t encode_size(const Ts&... vs) {
  datastream<size_t> ds;
  encode_size(ds, vs...);
  return ds.tellp();
}

template<typename T>
std::vector<char> encode(const T& v) {
  auto buffer = std::vector<char>(encode_size(v));
  datastream<char> ds(buffer);
  ds << v;
  return buffer;
}

template<typename Stream, typename T>
void encode(datastream<Stream>& ds, const T& v) {
  ds << v;
}

template<typename Stream, typename T, typename... Ts>
void encode(datastream<Stream>& ds, const T& v, const Ts&... vs) {
  ds << v;
  encode(ds, vs...);
}

template<typename... Ts>
std::vector<char> encode(const Ts&... vs) {
  auto buffer = std::vector<char>(encode_size(vs...));
  datastream<char> ds(buffer);
  encode(ds, vs...);
  return buffer;
}

template<typename Stream, typename T>
void decode(std::span<const char> s, T& t) {
  datastream<const char> ds(s);
  ds >> t;
}

template<typename Stream, typename T>
void decode(datastream<Stream>& ds, T& t) {
  ds >> t;
}

template<typename... Ts>
void decode(std::span<const char> s, Ts&... ts) {
  datastream<const char> ds(s);
  decode(ds, ts...);
}

const char term[] = {0x00, 0x01};
const char lit00[] = {0x00, char(0xff)};
const char litff[] = {char(0xff), 0x00};
const char inf[] = {char(0xff), char(0xff)};
const char msb[] = {0x00, char(0x80), char(0xc0), char(0xe0), char(0xf0), char(0xf8), char(0xfc), char(0xfe)};

const char increasing = 0x00;
const char decreasing = 0xff;

template<typename T>
struct decr {
  T val;

  bool operator==(const decr<T>& o) const {
    return val == o.val;
  }

  bool operator==(decr<T> o) {
    return val == o.val;
  }
};

struct infinity {};

struct trailing_string : std::string {};

struct string_or_infinity {
  std::string s;
  bool inf;
};

void invert(std::span<unsigned char>& s) {
  std::for_each(s.begin(), s.end(), [](unsigned char& c) { c ^= 0xff; });
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, infinity v) {
  ds.put(inf[0]);
  ds.put(inf[1]);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const trailing_string& v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string& v) {
  auto l = 0;
  for (auto i = 0; i < v.size(); i++) {
    switch ((unsigned char)v[i]) {
    case 0x00:
      ds.write(&v[l], i - l);
      ds.write(&lit00[0], 2);
      l = i + 1;
      break;
    case 0xff:
      ds.write(&v[l], i - l);
      ds.write(&litff[0], 2);
      l = i + 1;
    }
  }
  ds.write(&v[l], v.size() - l);
  ds.write(&term[0], 2);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, string_or_infinity v) {
  if (v.inf) {
    if (!v.s.empty()) {
      throw std::runtime_error("orderedcode: string_or_infinity has non-zero string and non-zero infinity");
    }
    ds << infinity{};
  } else {
    ds << v.s;
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, int64_t v) {
  if (v >= -64 && v < 64) {
    ds.put(static_cast<unsigned char>(v ^ 0x80));
    return ds;
  }
  bool neg = v < 0;
  if (neg) {
    v = ~v;
  }
  auto n = 1;
  std::vector<unsigned char> buf(10);
  auto i = 9;
  for (; v > 0; v >>= 8) {
    buf[i--] = static_cast<unsigned char>(v);
    n++;
  }
  bool lfb = n > 7;
  if (lfb) {
    n -= 7;
  }
  if (buf[i + 1] < 1 << (8 - n)) {
    n--;
    i++;
  }
  buf[i] |= msb[n];
  if (lfb) {
    buf[--i] = 0xff;
  }
  if (neg) {
    std::span<unsigned char> sp(&buf[i], buf.size() - i);
    invert(sp);
  }
  ds.write(&buf[i], buf.size() - i);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, uint64_t v) {
  std::vector<unsigned char> buf(9);
  auto i = 8;
  for (; v > 0; v >>= 8) {
    buf[i--] = static_cast<unsigned char>(v);
  }
  buf[i] = static_cast<unsigned char>(8 - i);
  ds.write(&buf[i], buf.size() - i);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, double v) {
  if (isnan(v)) {
    throw std::runtime_error("append: NaN");
  }
  uint64_t b;
  memcpy(&b, &v, sizeof(v));
  auto i = int64_t(b);
  if (i < 0) {
    i = std::numeric_limits<int64_t>::min() - i;
  }
  return ds << i;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, decr<T> v) {
  auto buffer = std::vector<unsigned char>(encode_size(v.val));
  datastream<unsigned char> ds2(buffer);
  ds2 << v.val;
  std::span<unsigned char> sp(buffer);
  invert(sp);

  ds.write(&buffer[0], buffer.size());
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, infinity& v) {
  if (ds.remaining() < 2) {
    throw std::runtime_error("orderedcode: corrupt input");
  }
  if (ds.get() != inf[0] || ds.get() != inf[1]) {
    throw std::runtime_error("orderedcode: corrupt input");
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<infinity>& v) {
  if (ds.remaining() < 2) {
    throw std::runtime_error("orderedcode: corrupt input");
  }
  if ((ds.get() ^ decreasing) != inf[0] || (ds.get() ^ decreasing) != inf[1]) {
    throw std::runtime_error("orderedcode: corrupt input");
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, trailing_string& v) {
  ds.read(v, ds.remaining());
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<trailing_string>& v) {
  std::vector<unsigned char> s;
  ds.read(s, ds.remaining());
  std::span<unsigned char> sp(s);
  invert(sp);
  v.val.insert(v.val.end(), s.begin(), s.end());
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::string& v) {
  v.clear();
  for (; ds.remaining() > 0;) {
    auto c = static_cast<const char>(ds.get());
    switch (static_cast<unsigned char>(c)) {
    case 0x00:
      if (ds.remaining() <= 0) {
        throw std::runtime_error("orderedcode: corrupt input");
      }
      switch (static_cast<unsigned char>(static_cast<const char>(ds.get()))) {
      case 0x01:
        return ds;
      case 0xff:
        v.insert(v.end(), static_cast<char>(0x00));
        break;
      default:
        throw std::runtime_error("orderedcode: corrupt input");
      }
      break;
    case 0xff:
      if (ds.remaining() <= 0 || ((static_cast<const char>(ds.get())) != static_cast<char>(0x00))) {
        throw std::runtime_error("orderedcode: corrupt input");
      }
      v.insert(v.end(), static_cast<char>(0xff));
      break;
    default:
      v.insert(v.end(), c);
    }
  }
  throw std::runtime_error("orderedcode: corrupt input");
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<std::string>& v) {
  v.val.clear();
  for (; ds.remaining() > 0;) {
    auto c = static_cast<const char>(ds.get());
    switch (static_cast<unsigned char>(c ^ decreasing)) {
    case 0x00:
      if (ds.remaining() <= 0) {
        throw std::runtime_error("orderedcode: corrupt input");
      }
      switch (static_cast<unsigned char>(static_cast<const char>(ds.get()) ^ decreasing)) {
      case 0x01:
        return ds;
      case 0xff:
        v.val.insert(v.val.end(), static_cast<char>(0x00));
        break;
      default:
        throw std::runtime_error("orderedcode: corrupt input");
      }
      break;
    case 0xff:
      if (ds.remaining() <= 0 || ((static_cast<const char>(ds.get()) ^ decreasing) != static_cast<char>(0x00))) {
        throw std::runtime_error("orderedcode: corrupt input");
      }
      v.val.insert(v.val.end(), static_cast<char>(0xff));
      break;
    default:
      v.val.insert(v.val.end(), static_cast<char>(c ^ decreasing));
    }
  }
  throw std::runtime_error("orderedcode: corrupt input");
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, string_or_infinity& v) {
  try {
    infinity _;
    ds >> _;
    v.inf = true;
    return ds;
  } catch (...) {
    ds >> v.s;
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<string_or_infinity>& v) {
  try {
    decr<infinity> _{};
    ds >> _;
    v.val.inf = true;
    return ds;
  } catch (...) {
    decr<std::string> s;
    ds >> s;
    v.val.s = s.val;
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, int64_t& v) {
  char dir = increasing;
  v = 0;
  auto c = static_cast<unsigned char>(ds.get() ^ dir);
  if (c >= 0x40 && c < 0xc0) {
    v = int64_t(int8_t(c ^ 0x80));
    return ds;
  }
  bool neg = (c & 0x80) == 0;
  if (neg) {
    c = ~c;
    dir = ~dir;
  }
  size_t n = 0;
  if (c == 0xff) {
    c = static_cast<unsigned char>(ds.get() ^ dir);
    if (c > 0xc0) {
      throw std::runtime_error("orderedcode: corrupt input");
    }
    n = 7;
  }
  for (unsigned char mask = 0x80; (c & mask) != 0; mask >>= 1) {
    c &= ~mask;
    n++;
  }
  int64_t x = c;
  for (size_t i = 1; i < n; i++) {
    c = static_cast<unsigned char>(ds.get() ^ dir);
    x = x << 8 | c;
  }
  if (neg) {
    x = ~x;
  }
  v = x;
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<int64_t>& v) {
  char dir = decreasing;
  v.val = 0;
  unsigned char c = ds.get() ^ dir;
  if (c >= 0x40 && c < 0xc0) {
    v.val = int64_t(int8_t(c ^ 0x80));
    return ds;
  }
  bool neg = (c & 0x80) == 0;
  if (neg) {
    c = ~c;
    dir = ~dir;
  }
  size_t n = 0;
  if (c == 0xff) {
    c = static_cast<unsigned char>(ds.get() ^ dir);
    if (c > 0xc0) {
      throw std::runtime_error("orderedcode: corrupt input");
    }
    n = 7;
  }
  for (unsigned char mask = 0x80; (c & mask) != 0; mask >>= 1) {
    c &= ~mask;
    n++;
  }
  int64_t x = c;
  for (size_t i = 1; i < n; i++) {
    c = ds.get() ^ dir;
    x = x << 8 | static_cast<unsigned char>(c);
  }
  if (neg) {
    x = ~x;
  }
  v.val = x;
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, uint64_t& v) {
  unsigned char n = ds.get();
  v = 0;
  for (size_t i = 0; i < n; i++) {
    v = v << 8 | static_cast<unsigned char>(ds.get());
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<uint64_t>& v) {
  unsigned char n = ds.get() ^ decreasing;
  v.val = 0;
  for (size_t i = 0; i < n; i++) {
    v.val = v.val << 8 | static_cast<unsigned char>(ds.get() ^ decreasing);
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, double& v) {
  int64_t i = 0;
  ds >> i;
  if (i < 0) {
    i = ((int64_t)-1 << 63) - i;
  }
  memcpy(&v, &i, sizeof(i));
  if (isnan(v)) {
    throw std::runtime_error("parse: NaN");
  }
  return ds;
}

template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, decr<double>& v) {
  decr<int64_t> i{int64_t(0)};
  ds >> i;
  if (i.val < 0) {
    i.val = ((int64_t)-1 << 63) - i.val;
  }
  memcpy(&v.val, &i.val, sizeof(i.val));
  if (isnan(v.val)) {
    throw std::runtime_error("parse: NaN");
  }
  return ds;
}

} // namespace noir::codec::orderedcode
