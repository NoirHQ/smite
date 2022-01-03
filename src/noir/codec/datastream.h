#pragma once
#include <span>
#include <stdexcept>
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

  inline auto& read(std::span<char> s) {
    if (!(remaining() >= s.size())) [[unlikely]]
      throw std::out_of_range("datastream attempted to read past the end");
    std::copy(pos_, pos_ + s.size(), s.begin());
    pos_ += s.size();
    return *this;
  }

  inline auto& read(char* s, size_t count) {
    return read({s, count});
  }

  inline int peek() {
    if (!(remaining() >= 1)) [[unlikely]]
      throw std::out_of_range("datastream attempted to read past the end");
    return *pos_;
  }

  inline int get() {
    auto c = peek();
    ++pos_;
    return c;
  }

  inline auto& get(char& c) {
    c = get();
    return *this;
  }

  inline auto& unget() {
    if (!(tellp() >= 1)) [[unlikely]]
      throw std::out_of_range("datastream attempted to read past the end");
    --pos_;
    return *this;
  }

  inline auto& write(std::span<const char> s) {
    if (!(remaining() >= s.size())) [[unlikely]]
      throw std::out_of_range("datastream attempted to write past the end");
    std::copy(s.begin(), s.end(), pos_);
    pos_ += s.size();
    return *this;
  }

  inline auto& write(const char* s, size_t count) {
    return write({s, count});
  }

  inline auto& write(const void* c, size_t count) {
    if (!(remaining() >= count)) [[unlikely]]
      throw std::out_of_range("datastream attempted to write past the end");
    auto s = std::span(reinterpret_cast<const char*>(c), count);
    std::copy(s.begin(), s.end(), pos_);
    pos_ += count;
    return *this;
  }

  inline auto& put(char c) {
    if (!(remaining() >= 1)) [[unlikely]]
      throw std::out_of_range("datastream attempted to write past the end");
    *pos_++ = c;
    return *this;
  }

  inline auto pos() const {
    return pos_;
  }

  inline auto& seekp(size_t p) {
    pos_ = span.begin() + p;
    return *this;
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

  inline void skip(size_t s) {
    size += s;
  }

  inline auto& write(std::span<const char> s) {
    size += s.size();
    return *this;
  }

  inline auto& write(const void*, size_t s) {
    size += s;
    return *this;
  }

  inline auto& put(char) {
    ++size;
    return *this;
  }

  inline auto& seekp(size_t p) {
    size = p;
    *this;
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
