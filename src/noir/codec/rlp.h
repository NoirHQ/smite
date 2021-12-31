#pragma once
#include <noir/codec/datastream.h>
#include <span>

namespace noir::codec {

class rlp {
public:
  template<typename Stream>
  using datastream = noir::codec::datastream<rlp, Stream>;

private:
  template<bool Prefix, typename Stream, typename T>
  static void encode(datastream<Stream>& ds, const T& v, unsigned char mod) {
    if constexpr (Prefix) {
      // encode a length prefix less than 55
      if (v <= 55) {
        ds.put(v + mod);
        return;
      }
    }
    auto s = std::span((const char*) &v, sizeof(T));
    auto nonzero = std::find_if(s.rbegin(), s.rend(), [](const auto& c) {
      if (c != 0)
        return true;
      return false;
    });
    auto trimmed = std::distance(nonzero, s.rend());
    if constexpr (!Prefix) {
      // encode a value less than 128 (0x80) as single byte
      if (trimmed == 1 && ((v & 0xff) < 0x80u)) {
        ds.put(v & 0xff);
        return;
      }
    } else {
      // adjust the modifier to encode a length prefix greater than 55
      // e.g. bytes (0x80 => 0xb7), lists (0xc0 => 0xf7)
      mod += 55;
    }
    ds.put(trimmed + mod);
    std::for_each(s.rend() - trimmed, s.rend(), [&](const auto& c) {
      ds.write(&c, 1);
    });
  }

  template<bool Prefix, typename Stream>
  static uint64_t decode(datastream<Stream>& ds, unsigned char prefix, unsigned char mod) {
    if constexpr (!Prefix) {
      // decode a value less than 128 (0x80)
      if (prefix < 0x80) {
        return prefix;
      }
    }
    auto size = prefix - mod;
    if constexpr (Prefix) {
      if (size <= 55) {
        // decode a length prefix less than 55
        return size;
      } else {
        // decode a length prefix greater than 55 (need to subtract 55 from modifier)
        size -= 55;
      }
    }
    auto v = 0ull;
    auto s = std::span((char*)&v, sizeof(v));
    std::for_each(s.rend() - size, s.rend(), [&](auto& c) {
      ds.read(&c, 1);
    });
    return v;
  }

public:
  template<typename Stream, typename T>
  static void encode_bytes(datastream<Stream>& ds, const T& v, unsigned char mod) {
    encode<0>(ds, v, mod);
  }

  template<typename Stream, typename T>
  static void encode_prefix(datastream<Stream>& ds, const T& v, unsigned char mod) {
    encode<1>(ds, v, mod);
  }

  template<typename Stream>
  static uint64_t decode_bytes(datastream<Stream>& ds, unsigned char v, unsigned char mod) {
    return decode<0>(ds, v, mod);
  }

  template<typename Stream>
  static uint64_t decode_prefix(datastream<Stream>& ds, unsigned char v, unsigned char mod) {
    return decode<1>(ds, v, mod);
  }
};

// integers
template<typename Stream, typename T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
datastream<rlp, Stream>& operator<<(datastream<rlp, Stream>& ds, const T& v) {
  static_assert(sizeof(T) <= 55);
  rlp::encode_bytes(ds, v, 0x80);
  return ds;
}

template<typename Stream, typename T, std::enable_if_t<std::numeric_limits<T>::is_integer, bool> = true>
datastream<rlp, Stream>& operator>>(datastream<rlp, Stream>& ds, T& v) {
  unsigned char prefix = 0;
  ds.read((char*)&prefix, 1);
  if (prefix < 0x80) {
    v = prefix;
  } else if (prefix < 0xb8) {
    check(prefix - 0x80 <= sizeof(T), "not sufficient output size");
    v = rlp::decode_bytes(ds, prefix, 0x80);
  } else if (prefix < 0xc0) {
    // TODO
    check(false, "not implemented");
  } else {
    check(false, "not matched prefix type");
  }
  return ds;
}

// list
template<typename Stream, typename T>
datastream<rlp, Stream>& operator<<(datastream<rlp, Stream>& ds, const std::vector<T>& v) {
  auto size = 0ull;
  for (const auto& val : v) {
    size += encode_size<rlp>(val);
  }
  rlp::encode_prefix(ds, size, 0xc0);
  for (const auto& val : v) {
    ds << val;
  }
  return ds;
}

template<typename Stream, typename T>
datastream<rlp, Stream>& operator>>(datastream<rlp, Stream>& ds, std::vector<T>& v) {
  unsigned char prefix = 0;
  ds.read((char*)&prefix, 1);
  check(prefix >= 0xc0, "not matched prefix type");
  auto size = rlp::decode_prefix(ds, prefix, 0xc0);
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
datastream<rlp, Stream>& operator<<(datastream<rlp, Stream>& ds, const std::string& v) {
  auto size = v.size();
  if (size == 1 && v[0] < 0x80u) {
    ds.put(v[0]);
  } else {
    rlp::encode_prefix(ds, size, 0x80);
    ds.write(v.data(), size);
  }
  return ds;
}

template<typename Stream>
datastream<rlp, Stream>& operator>>(datastream<rlp, Stream>& ds, std::string& v) {
  unsigned char prefix = 0;
  ds.read((char*)&prefix, 1);
  if (prefix < 0x80) {
    v.resize(1);
    v[0] = prefix;
  } else if (prefix < 0xc0) {
    auto size = rlp::decode_prefix(ds, prefix, 0x80);
    v.resize(size);
    ds.read(v.data(), size);
  } else {
    check(false, "not matched prefix type");
  }
  return ds;
}

} // namespace noir::codec
