#pragma once
#include <span>
#include <stdexcept>
#include <vector>

namespace noir::codec {

template<typename T>
class basic_datastream {
public:
  basic_datastream(std::span<T> s)
    : span(s), pos_(s.begin()) {}

  basic_datastream(T* s, size_t count)
    : span(s, count), pos_(span.begin()) {}

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


template<>
class basic_datastream<size_t> {
public:
  constexpr basic_datastream(size_t init_size = 0)
    : size(init_size) {}

  constexpr void skip(size_t s) {
    size += s;
  }

  constexpr auto& write(std::span<const char> s) {
    size += s.size();
    return *this;
  }

  constexpr auto& write(const void*, size_t s) {
    size += s;
    return *this;
  }

  constexpr auto& put(char) {
    ++size;
    return *this;
  }

  constexpr auto& seekp(size_t p) {
    size = p;
    return *this;
  }

  constexpr size_t tellp() const {
    return size;
  }

  constexpr size_t remaining() const {
    return 0;
  }

private:
  size_t size;
};

} // namespace noir::codec

#define NOIR_CODEC(CODEC) \
namespace noir::codec::CODEC { \
template<typename T> \
class datastream : public basic_datastream<T> { \
public: \
  using basic_datastream<T>::basic_datastream; \
}; \
template<typename T> \
constexpr size_t encode_size(const T& v) { \
  datastream<size_t> ds; \
  ds << v; \
  return ds.tellp(); \
} \
template<typename T> \
std::vector<char> encode(const T& v) { \
  auto buffer = std::vector<char>(encode_size(v)); \
  datastream<char> ds(buffer); \
  ds << v; \
  return buffer; \
} \
template<typename T> \
T decode(std::span<const char> s) { \
  T v; \
  datastream<const char> ds(s); \
  ds >> v; \
  return v; \
} \
} \
namespace noir::codec::CODEC

