// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/common/types/bytes.h>
#include <bitset>
#include <numeric>
#include <span>

namespace noir {

/// \brief thin wrapper for fixed-length byte sequence
/// \tparam N length of byte sequence
/// \tparam T byte-compatible type
/// \ingroup common
template<size_t N, Byte T = byte_type>
class bytes_n {
private:
  std::array<T, N> data_;

public:
  using underlying_type = std::array<T, N>;
  using value_type = typename underlying_type::value_type;
  using size_type = typename underlying_type::size_type;
  using difference_type = typename underlying_type::difference_type;
  using reference = typename underlying_type::reference;
  using const_reference = typename underlying_type::const_reference;
  using pointer = typename underlying_type::pointer;
  using const_pointer = typename underlying_type::const_pointer;
  using iterator = typename underlying_type::iterator;
  using const_iterator = typename underlying_type::const_iterator;
  using reverse_iterator = typename underlying_type::reverse_iterator;
  using const_reverse_iterator = typename underlying_type::const_reverse_iterator;

  constexpr bytes_n() = default;

  /// \brief constructs bytes_n from hex string
  /// \param s hex string, must have the length of 2 * N unless \p canonical is false
  /// \param canonical whether the input generates the exact size of bytes_n
  constexpr bytes_n(std::string_view s, bool canonical = true) {
    auto size = from_hex(s, data_);
    check(!canonical || size == N, "invalid bytes length: expected({}), actual({})", N, size);
    if (size < N) {
      std::fill(data_.begin() + size, data_.end(), 0);
    }
  }

  /// \brief constructs bytes_n from byte sequence
  /// \param bytes byte sequence, must have the size of \p N unless \p canonical is false
  /// \param canonical whether the input generates the exact size of bytes_n
  template<Byte U, size_t S>
  constexpr bytes_n(std::span<U, S> bytes, bool canonical = true) {
    auto size = bytes.size();
    check(!canonical || size == N, "invalid bytes length: expected({}), actual({})", N, size);
    std::copy(bytes.begin(), bytes.end(), data_.begin());
    if (size < N) {
      std::fill(data_.begin() + size, data_.end(), 0);
    }
  }

  /// \brief constructs bytes_n from pointer to byte sequence and its size
  // XXX: trailing "//" in the next line forbids clang-format from erasing line break.
  [[deprecated("ambiguous, consider using bytes_n(std::string_view) or bytes_n(std::span)")]] //
  constexpr bytes_n(const char* data, size_t size, bool canonical = true)
    : bytes_n(std::span{data, size}, canonical) {}

  /// \brief constructs bytes_n from byte vector
  template<Byte U>
  constexpr bytes_n(const std::vector<U>& bytes, bool canonical = true)
    : bytes_n(std::span{(T*)bytes.data(), bytes.size()}, canonical) {}

  constexpr reference at(size_type pos) {
    return data_.at(pos);
  }
  constexpr const_reference at(size_type pos) const {
    return data_.at(pos);
  }
  constexpr reference operator[](size_type pos) {
    return data_[pos];
  }
  constexpr const_reference operator[](size_type pos) const {
    return data_[pos];
  }
  constexpr reference front() {
    return data_.front();
  }
  constexpr const_reference front() const {
    return data_.front();
  }
  constexpr reference back() {
    return data_.back();
  }
  constexpr const_reference back() const {
    return data_.back();
  }

  constexpr T* data() noexcept {
    return data_.data();
  }
  constexpr const T* data() const noexcept {
    return data_.data();
  }
  constexpr const size_type size() const noexcept {
    return data_.size();
  }
  [[nodiscard]] bool empty() const noexcept {
    if constexpr (!N) {
      return true;
    }
    for (auto i = 0; i < size(); ++i) {
      if (data_[i]) {
        return false;
      }
    }
    return true;
  }

  constexpr iterator begin() noexcept {
    return data_.begin();
  }
  constexpr const_iterator begin() const noexcept {
    return data_.begin();
  }
  constexpr iterator end() noexcept {
    return data_.end();
  }
  constexpr const_iterator end() const noexcept {
    return data_.end();
  }

  constexpr reverse_iterator rbegin() noexcept {
    return data_.rbegin();
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return data_.rbegin();
  }
  constexpr reverse_iterator rend() noexcept {
    return data_.rend();
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return data_.rend();
  }

  std::string to_string() const {
    return to_hex({(const char*)data_.data(), data_.size()});
  }

  bytes to_bytes() const {
    return bytes{data_.begin(), data_.end()};
  }

  template<Byte U = T, size_t S = std::dynamic_extent>
  constexpr std::span<U, S> to_span() {
    return std::span((U*)data_.data(), data_.size());
  }

  template<Byte U = T, size_t S = std::dynamic_extent>
  constexpr std::span<const U, S> to_span() const {
    return std::span((const U*)data_.data(), data_.size());
  }

  constexpr std::bitset<8 * N> to_bitset() const {
    std::bitset<8 * N> out;
    return std::accumulate(data_.begin(), data_.end(), out, [](auto out, uint8_t v) {
      decltype(out) current(v);
      return (std::move(out) << 8) | current;
    });
  }

  void clear() {
    std::fill(data_.begin(), data_.end(), 0);
  }

  template<size_t S, Byte U>
  constexpr bool operator==(const bytes_n<S, U>& v) const {
    if constexpr (N != S) {
      return false;
    }
    return !std::memcmp(data(), v.data(), N);
  }

  template<size_t S, Byte U>
  constexpr bool operator!=(const bytes_n<S, U>& v) const {
    if constexpr (N != S) {
      return true;
    }
    return std::memcmp(data(), v.data(), N);
  }

  template<size_t S, Byte U>
  constexpr bool operator<(const bytes_n<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp < 0) {
      return true;
    } else if (!cmp) {
      return N < S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator<=(const bytes_n<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp < 0) {
      return true;
    } else if (!cmp) {
      return N <= S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator>(const bytes_n<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp > 0) {
      return true;
    } else if (!cmp) {
      return N > S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator>=(const bytes_n<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp > 0) {
      return true;
    } else if (!cmp) {
      return N >= S;
    }
    return false;
  }
};

template<typename DataStream, size_t N, Byte U = byte_type>
DataStream& operator<<(DataStream& ds, const bytes_n<N, U>& v) {
  ds << v.to_span();
  return ds;
}

template<typename DataStream, size_t N, Byte U = byte_type>
DataStream& operator>>(DataStream& ds, bytes_n<N, U>& v) {
  ds >> v.to_span();
  return ds;
}

template<size_t N, Byte U = byte_type>
std::ostream& operator<<(std::ostream& ds, const bytes_n<N, U>& v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<size_t N, Byte U = byte_type>
std::istream& operator>>(std::istream& ds, bytes_n<N, U>& v) {
  ds.read(v.data(), v.size());
  return ds;
}

using bytes20 = bytes_n<20>;
using bytes32 = bytes_n<32>;
using bytes256 = bytes_n<256>;

template<>
struct is_foreachable<bytes20> : std::false_type {};
template<>
struct is_foreachable<bytes32> : std::false_type {};
template<>
struct is_foreachable<bytes256> : std::false_type {};

} // namespace noir