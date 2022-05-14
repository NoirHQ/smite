// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/concepts.h>
#include <noir/common/hex.h>
#include <noir/common/mem_clr.h>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace noir {

// concept for byte compatible type
template<typename T, typename U = Deref<T>>
concept Byte = std::is_same_v<U, unsigned char> || std::is_same_v<U, char> || std::is_same_v<U, std::byte>;

// concept for a pointer to byte compatible type
template<typename T>
concept BytePtr = Byte<T> && std::is_pointer_v<T>;

// not-owned readonly byte sequence
using BytesView = std::span<const unsigned char>;

// not-owned mutable byte sequence
using BytesViewMut = std::span<unsigned char>;

// concept for convertible to bytes view (no implicit)
template<typename T>
concept BytesViewConstructible = !std::is_convertible_v<T, BytesView> && requires (T v) {
  { v.data() } -> BytePtr;
  { v.size() } -> ConvertibleTo<size_t>;
};

auto bytes_view(BytesViewConstructible auto& bytes) -> BytesView {
  return {reinterpret_cast<BytesView::pointer>(bytes.data()), bytes.size()};
}

auto bytes_view_mut(BytesViewConstructible auto& bytes) -> BytesViewMut {
  return {reinterpret_cast<BytesViewMut::pointer>(bytes.data()), bytes.size()};
}


// fixed or dynamic sized byte sequence
template<size_t N>
class BytesN {
private:
  template<size_t S>
  struct Backend {
    using type = std::array<unsigned char, S>;
  };
  template<>
  struct Backend<std::dynamic_extent> {
    using type = std::vector<unsigned char>;
  };

  // array used for fixed, vector used for dynamic
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

  BytesN() = default;

  BytesN(std::string_view s, bool canonical = true) {
    if constexpr (N != std::dynamic_extent) {
      auto size = hex::decode(backend.data(), backend.size(), s);
      if (canonical && size != N) {
        throw std::runtime_error("invalid bytes length");
      }
      if (size < N) {
        std::fill(backend.begin() + size, backend.end(), 0);
      }
    } else {
      auto vec = hex::decode(s);
      std::swap(backend, vec);
    }
  }

  template<typename T>
  requires (ConvertibleTo<T, BytesView> || BytesViewConstructible<T>)
  BytesN(T&& bytes, bool canonical = true) {
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

  BytesN(std::vector<unsigned char>& vec, bool canonical = true): BytesN(std::span(vec), canonical) {}

  BytesN(std::vector<unsigned char>&& vec) requires (N == std::dynamic_extent) {
    std::swap(backend, vec);
  }

  BytesN(size_t size) requires (N == std::dynamic_extent)
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
    return hex::encode(backend);
  }

  void clear() {
    std::fill(backend.begin(), backend.end(), 0);
  }
};

// byte sequence clearing storage during destruction
template<size_t N>
class SecureBytesN : public BytesN<N> {
private:
  using super = BytesN<N>;

public:
  ~SecureBytesN() {
    mem_cleanse(super::data(), super::size());
  }
};

// alias name for dynamic byte sequence
using Bytes = BytesN<std::dynamic_extent>;

// alias name for secure dynamic byte sequence
using SecureBytes = SecureBytesN<std::dynamic_extent>;

} // namespace noir
