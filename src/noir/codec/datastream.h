#pragma once
#include <noir/common/check.h>
#include <span>
#include <vector>

namespace noir::codec {

template<typename Codec, typename T>
class datastream {
public:
  datastream(std::span<T> s)
    : span(s), pos_(s.begin()) {}

  inline void skip(size_t s) {
    pos_ += s;
  }

  inline bool read(std::span<char> s) {
    check(std::distance(pos_, span.end()) >= s.size(), "datastream attempted to read past the end");
    std::copy(pos_, pos_ + s.size(), s.begin());
    pos_ += s.size();
    return true;
  }

  inline bool write(std::span<char> s) {
    check(std::distance(pos_, span.end()) >= s.size(), "datastream attempted to write past the end");
    std::copy(s.begin(), s.end(), pos_);
    pos_ += s.size();
    return true;
  }

  inline bool write(char c) {
    check(std::distance(pos_, span.end()) >= 1, "datastream attempted to write past the end");
    *pos_++ = c;
    return true;
  }

  inline bool write(const void* c, size_t count) {
    check(std::distance(pos_, span.end()) >= count, "datastream attempted to write pas the end");
    auto s = std::span<const char>(reinterpret_cast<const char*>(c), count);
    std::copy(s.begin(), s.end(), pos_);
    pos_ += count;
    return true;
  }

  inline bool put(char c) {
    check(std::distance(pos_, span.end()), "put");
    *pos_++ = c;
    return true;
  }

  auto pos() const {
    return pos_;
  }

  inline bool seekp(size_t p) {
    pos_ = span.begin() + p;
    return true;
  }

  inline size_t tellp() const {
    return std::distance(span.begin(), pos_);
  }

  inline size_t remaining() const {
    return std::distance(pos_, span.end());
  }

private:
  std::span<T> span;
  typename std::span<T>::iterator pos_;
};


template<typename Codec>
class datastream<Codec, size_t> {
public:
  datastream(size_t init_size = 0)
    : size(init_size) {}

  inline bool skip(size_t s) {
    size += s;
    return true;
  }

  inline bool write(std::span<char> s) {
    size += s.size();
    return true;
  }

  inline bool write(char) {
    ++size;
    return true;
  }

  inline bool write(const void*, size_t s) {
    size += s;
    return true;
  }

  inline bool put(char) {
    ++size;
    return true;
  }

  inline bool seekp(size_t p) {
    size = p;
    return true;
  }

  inline size_t tellp() const {
    return size;
  }

  inline size_t remaining() const {
    return 0;
  }

private:
  size_t size;
};

template<typename Codec, typename T, typename U>
datastream<Codec, T>& operator<<(datastream<Codec, T>& ds, const U& v);

template<typename Codec, typename T, typename U>
datastream<Codec, T>& operator>>(datastream<Codec, T>& ds, U& v);

template<typename Codec, typename T>
size_t encode_size(const T& v) {
  datastream<Codec, size_t> ds;
  ds << v;
  return ds.tellp();
}

template<typename Codec, typename T>
std::vector<char> encode(const T& v) {
  auto size = encode_size<Codec>(v);
  std::vector<char> result(size);
  if (size) {
    datastream<Codec, char> ds(result);
    ds << v;
  }
  return result;
}

template<typename Codec, typename T>
T decode(std::span<const char> buffer) {
  T result;
  datastream<Codec, const char> ds(buffer);
  ds >> result;
  return result;
}

} // namespace noir::codec
