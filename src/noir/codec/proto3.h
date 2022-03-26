// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/codec/proto3/types.h>
#include <noir/common/concepts.h>
#include <noir/common/refl.h>

namespace noir::codec::proto3 {

template<typename T>
class datastream : public basic_datastream<T> {
public:
  using basic_datastream<T>::basic_datastream;
  uint32_t tag = 0;
};
template<typename T>
constexpr size_t encode_size(const T& v, uint32_t tag = 0) {
  datastream<size_t> ds;
  ds.tag = tag;
  ds << v;
  return ds.tellp();
}
template<typename T>
std::vector<char> encode(const T& v) {
  auto buffer = std::vector<char>(encode_size(v));
  datastream<char> ds(buffer);
  ds << v;
  return buffer;
}
template<typename T>
T decode(std::span<const char> s) {
  T v{};
  datastream<const char> ds(s);
  ds >> v;
  return v;
}

template<typename Stream, integral T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  if (v != 0) {
    write_uleb128(ds, varint<T>(v));
  }
  return ds;
}

template<typename Stream, integral T>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  v = 0;
  if (ds.remaining()) {
    varint<T> val;
    read_uleb128(ds, val);
    v = val;
    check(v, "0 should not be encoded");
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const varint<T>& v) {
  if (v != 0) {
    write_uleb128(ds, v);
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, varint<T>& v) {
  v = 0;
  if (ds.remaining()) {
    read_uleb128(ds, v);
    check(v, "0 should not be encoded");
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const sint<T>& v) {
  if (v != 0) {
    write_zigzag(ds, v);
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, sint<T>& v) {
  v = 0;
  if (ds.remaining()) {
    read_zigzag(ds, v);
    check(v, "0 should not be encoded");
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const fixed<T>& v) {
  if (v != 0) {
    ds.write((const char*)&v.value, sizeof(T));
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, fixed<T>& v) {
  v = 0;
  if (ds.remaining()) {
    ds.read((char*)&v.value, sizeof(T));
    check(v, "0 should not be encoded");
  }
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

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string& v) {
  if (v.size() > 0) {
    ds.write(v.data(), v.size());
  }
  return ds;
}

// for decoding (wire_type == 2), passed datastream should have bytes only for output
template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::string& v) {
  auto size = ds.remaining();
  v.resize(size);
  ds.read(v.data(), size);
  return ds;
}

template<typename Stream>
datastream<Stream>& operator<<(datastream<Stream>& ds, const bytes& v) {
  if (v.size() > 0) {
    ds.write(v.data(), v.size());
  }
  return ds;
}

// for decoding (wire_type == 2), passed datastream should have bytes only for output
template<typename Stream>
datastream<Stream>& operator>>(datastream<Stream>& ds, bytes& v) {
  auto size = ds.remaining();
  v.resize(size);
  ds.read(v.data(), size);
  return ds;
}

template<typename Stream, integral T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::vector<T>& v) {
  for (const auto& e : v) {
    ds << e;
  }
  return ds;
}

// for decoding (wire_type == 2), passed datastream should have bytes only for output
template<typename Stream, integral T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::vector<T>& v) {
  while (ds.remaining()) {
    T tmp;
    ds >> tmp;
    v.push_back(tmp);
  }
  return ds;
}

// repeated fields
template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::vector<T>& v) {
  check(ds.tag, "tag should not be 0");
  for (const auto& e : v) {
    auto type = wire_type_v<T>;
    auto size = encode_size(e);
    if (size > 0) {
      ds.put((ds.tag << 3) | type);
      if (type == 2) {
        ds << size;
      }
      ds << e;
    }
  }
  ds.tag = 0;
  return ds;
}

// repeated fields
template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::vector<T>& v) {
  if (ds.remaining()) {
    T tmp;
    ds >> tmp;
    v.push_back(tmp);
  }
  return ds;
}

template<typename Stream, reflection T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const T& v) {
  auto fields_count = refl::fields_count_v<T>;
  auto tag = 1;
  while (fields_count && tag < max_tag) {
    refl::for_each_field(
      [&](const auto& desc, const auto& value) {
        if (desc.tag == tag) {
          auto type = wire_type_v<std::decay_t<decltype(value)>>;
          auto size = encode_size(value, (type == repeated_type) ? tag : 0);
          if (size > 0) {
            if (type == repeated_type) {
              ds.tag = tag;
            } else {
              ds.put((desc.tag << 3) | type);
              if (type == 2) {
                ds << size;
              }
            }
            ds << value;
          }
          fields_count -= 1;
          return false;
        }
        return true;
      },
      v);
    tag += 1;
  }
  return ds;
}

// for decoding (wire_type == 2), passed datastream should have bytes only for output
template<typename Stream, reflection T>
datastream<Stream>& operator>>(datastream<Stream>& ds, T& v) {
  if (ds.remaining()) {
    auto fields_count = refl::fields_count_v<T>;
    auto processed = 0;
    while (fields_count) {
      varuint32 key;
      read_uleb128(ds, key);
      auto tag = key.value >> 3;
      // for canonical representation, original specification doesn't have this limitation.
      check(tag >= processed, "not encoded in canonical sequence");
      processed = tag;
      auto type = key & 0b111;
      auto ret = refl::for_each_field(
        [&](const auto& desc, auto& value) {
          if (desc.tag == tag) {
            auto w_type = wire_type_v<std::decay_t<decltype(value)>>;
            check(type == w_type || w_type == repeated_type, "wire type unmatched");
            if (type == 2) {
              size_t size = 0;
              ds >> size;
              check(size > 0, "empty bytes should not encoded");
              datastream<Stream> subds(ds.subspan(ds.tellp(), size));
              subds >> value;
              ds.skip(size);
            } else {
              ds >> value;
            }
            if (w_type != repeated_type) {
              fields_count -= 1;
            }
            return false;
          }
          return true;
        },
        v);
      check(!ret, "field not found with tag: {}", tag);
      if (ds.remaining() == 0) {
        break;
      }
    }
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator<<(datastream<Stream>& ds, const std::optional<T>& v) {
  if (v) {
    ds << *v;
  }
  return ds;
}

template<typename Stream, typename T>
datastream<Stream>& operator>>(datastream<Stream>& ds, std::optional<T>& v) {
  T tmp;
  ds >> tmp;
  v = std::forward<T>(tmp);
  return ds;
}

} // namespace noir::codec::proto3
