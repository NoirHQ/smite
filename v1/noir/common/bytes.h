#pragma once
#include <noir/common/mem_clr.h>
#include <cppcodec/hex_lower.hpp>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace noir {

template<size_t N = std::dynamic_extent>
class Bytes {
private:
  template<size_t S>
  struct Backend {
    using type = std::array<unsigned char, S>;
  };
  template<>
  struct Backend<std::dynamic_extent> {
    using type = std::vector<unsigned char>;
  };

  typename Backend<N>::type backend;

public:
  using raw_type = decltype(backend);
  using value_type = typename raw_type::value_type;
  using size_type = typename raw_type::size_type;
  using difference_type = typename raw_type::difference_type;
  using reference = typename raw_type::reference;
  using const_reference = typename raw_type::const_reference;
  using pointer = typename raw_type::pointer;
  using const_pointer = typename raw_type::const_pointer;
  using iterator = typename raw_type::iterator;
  using const_iterator = typename raw_type::const_iterator;
  using reverse_iterator = typename raw_type::reverse_iterator;
  using const_reverse_iterator = typename raw_type::const_reverse_iterator;

  Bytes() = default;

  Bytes(std::string_view s, bool canonical = true) {
    if constexpr (N != std::dynamic_extent) {
      auto size = cppcodec::hex_lower::decode(backend.data(), backend.size(), s);
      if (canonical && size != N) {
        throw std::runtime_error("invalid bytes length");
      }
      if (size < N) {
        std::fill(backend.begin() + size, backend.end(), 0);
      }
    } else {
      auto vec = cppcodec::hex_lower::decode(s);
      std::swap(backend, vec);
    }
  }

  template<typename T, size_t S>
  Bytes(std::span<T, S> bytes, bool canonical = true) {
    if constexpr (N != std::dynamic_extent) {
      auto size = bytes.size();
      if (canonical && size != N) {
        throw std::runtime_error("invalid bytes length");
      }
      std::copy(bytes.begin(), bytes.end(), backend.begin());
      if (size < N) {
        std::fill(backend.begin() + size, backend.end(), 0);
      }
    } else {
      backend = {bytes.begin(), bytes.end()};
    }
  }

  Bytes(std::vector<unsigned char>& vec, bool canonical = true): Bytes(std::span(vec), canonical) {}

  Bytes(std::vector<unsigned char>&& vec) requires (N == std::dynamic_extent) {
    std::swap(backend, vec);
  }

  Bytes(size_t size) requires (N == std::dynamic_extent)
    : backend(size) {}

  raw_type& raw() {
    return backend;
  }
  const raw_type& raw() const {
    return backend;
  }

  reference at(size_type pos) {
    return backend.at(pos);
  }
  const_reference at(size_type pos) const {
    return backend.at(pos);
  }
  reference operator[](size_type pos) {
    return backend[pos];
  }
  const_reference operator[](size_type pos) const {
    return backend[pos];
  }

  reference front() {
    return backend.front();
  }
  const_reference front() const {
    return backend.front();
  }
  reference back() {
    return backend.back();
  }
  const_reference back() const {
    return backend.back();
  }

  iterator begin() noexcept {
    return backend.begin();
  }
  const_iterator begin() const noexcept {
    return backend.begin();
  }
  iterator end() noexcept {
    return backend.end();
  }
  const_iterator end() const noexcept {
    return backend.end();
  }

  iterator rbegin() noexcept {
    return backend.rbegin();
  }
  const_iterator rbegin() const noexcept {
    return backend.rbegin();
  }
  iterator rend() noexcept {
    return backend.rend();
  }
  const_iterator rend() const noexcept {
    return backend.rend();
  }

  pointer data() noexcept {
    return backend.data();
  }
  const_pointer data() const noexcept {
    return backend.data();
  }
  size_type size() const noexcept {
    return backend.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    if constexpr (!N) {
      return true;
    } else if constexpr (N == 1) {
      return !backend[0];
    }
    for (auto i = 0; i < size(); ++i) {
      if (backend[i]) {
        return false;
      }
    }
    return true;
  }
  void resize(size_type size) requires (N == std::dynamic_extent) {
    backend.resize(size);
  }

  std::string to_string() const {
    return cppcodec::hex_lower::encode(backend);
  }

  void clear() {
    std::fill(backend.begin(), backend.end(), 0);
  }
};

template<size_t N = std::dynamic_extent>
class SecureBytes : public Bytes<N> {
private:
  using super = Bytes<N>;

public:
  ~SecureBytes() {
    mem_cleanse(super::data(), super::size());
  }
};

} // namespace noir
