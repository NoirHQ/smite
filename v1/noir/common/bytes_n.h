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
#include <noir/common/bytes.h>
#include <bitset>
#include <numeric>
#include <span>

namespace noir {

/// \brief thin wrapper for fixed-length byte sequence
/// \tparam N length of byte sequence
/// \tparam T byte-compatible type
/// \ingroup common
template<size_t N, Byte T = ByteType>
class BytesN {
private:
  std::array<T, N> data_;

public:
  using UnderlyingType = std::array<T, N>;
  using ValueType = typename UnderlyingType::value_type;
  using SizeType = typename UnderlyingType::size_type;
  using DifferenceType = typename UnderlyingType::difference_type;
  using Reference = typename UnderlyingType::reference;
  using ConstReference = typename UnderlyingType::const_reference;
  using Pointer = typename UnderlyingType::pointer;
  using ConstPointer = typename UnderlyingType::const_pointer;
  using Iterator = typename UnderlyingType::iterator;
  using ConstIterator = typename UnderlyingType::const_iterator;
  using ReverseIterator = typename UnderlyingType::reverse_iterator;
  using ConstReverseIterator = typename UnderlyingType::const_reverse_iterator;

  constexpr BytesN() = default;

  /// \brief constructs bytes_n from hex string
  /// \param s hex string, must have the length of 2 * N unless \p canonical is false
  /// \param canonical whether the input generates the exact size of bytes_n
  constexpr BytesN(std::string_view s, bool canonical = true) {
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
  constexpr BytesN(std::span<U, S> bytes, bool canonical = true) {
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
  constexpr BytesN(const char* data, size_t size, bool canonical = true)
    : BytesN(std::span{data, size}, canonical) {}

  /// \brief constructs bytes_n from byte vector
  template<Byte U>
  constexpr BytesN(const std::vector<U>& bytes, bool canonical = true)
    : BytesN(std::span{(T*)bytes.data(), bytes.size()}, canonical) {}

  constexpr Reference at(SizeType pos) {
    return data_.at(pos);
  }
  constexpr ConstReference at(SizeType pos) const {
    return data_.at(pos);
  }
  constexpr Reference operator[](SizeType pos) {
    return data_[pos];
  }
  constexpr ConstReference operator[](SizeType pos) const {
    return data_[pos];
  }
  constexpr Reference front() {
    return data_.front();
  }
  constexpr ConstReference front() const {
    return data_.front();
  }
  constexpr Reference back() {
    return data_.back();
  }
  constexpr ConstReference back() const {
    return data_.back();
  }

  constexpr T* data() noexcept {
    return data_.data();
  }
  constexpr const T* data() const noexcept {
    return data_.data();
  }
  constexpr const SizeType size() const noexcept {
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

  constexpr Iterator begin() noexcept {
    return data_.begin();
  }
  constexpr ConstIterator begin() const noexcept {
    return data_.begin();
  }
  constexpr Iterator end() noexcept {
    return data_.end();
  }
  constexpr ConstIterator end() const noexcept {
    return data_.end();
  }

  constexpr ReverseIterator rbegin() noexcept {
    return data_.rbegin();
  }
  constexpr ConstReverseIterator rbegin() const noexcept {
    return data_.rbegin();
  }
  constexpr ReverseIterator rend() noexcept {
    return data_.rend();
  }
  constexpr ConstReverseIterator rend() const noexcept {
    return data_.rend();
  }

  std::string to_string() const {
    return to_hex({(const char*)data_.data(), data_.size()});
  }

  Bytes to_bytes() const {
    return Bytes{data_.begin(), data_.end()};
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
  constexpr bool operator==(const BytesN<S, U>& v) const {
    if constexpr (N != S) {
      return false;
    }
    return !std::memcmp(data(), v.data(), N);
  }

  template<size_t S, Byte U>
  constexpr bool operator!=(const BytesN<S, U>& v) const {
    if constexpr (N != S) {
      return true;
    }
    return std::memcmp(data(), v.data(), N);
  }

  template<size_t S, Byte U>
  constexpr bool operator<(const BytesN<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp < 0) {
      return true;
    } else if (!cmp) {
      return N < S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator<=(const BytesN<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp < 0) {
      return true;
    } else if (!cmp) {
      return N <= S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator>(const BytesN<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp > 0) {
      return true;
    } else if (!cmp) {
      return N > S;
    }
    return false;
  }

  template<size_t S, Byte U>
  constexpr bool operator>=(const BytesN<S, U>& v) const {
    auto cmp = std::memcmp(data(), v.data(), std::min(N, S));
    if (cmp > 0) {
      return true;
    } else if (!cmp) {
      return N >= S;
    }
    return false;
  }
};

template<typename DataStream, size_t N, Byte U = ByteType>
DataStream& operator<<(DataStream& ds, const BytesN<N, U>& v) {
  ds << v.to_span();
  return ds;
}

template<typename DataStream, size_t N, Byte U = ByteType>
DataStream& operator>>(DataStream& ds, BytesN<N, U>& v) {
  ds >> v.to_span();
  return ds;
}

template<size_t N, Byte U = ByteType>
std::ostream& operator<<(std::ostream& ds, const BytesN<N, U>& v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<size_t N, Byte U = ByteType>
std::istream& operator>>(std::istream& ds, BytesN<N, U>& v) {
  ds.read(v.data(), v.size());
  return ds;
}

using Bytes20 = BytesN<20>;
using Bytes32 = BytesN<32>;
using Bytes256 = BytesN<256>;

template<>
struct is_foreachable<Bytes20> : std::false_type {};
template<>
struct is_foreachable<Bytes32> : std::false_type {};
template<>
struct is_foreachable<Bytes256> : std::false_type {};

} // namespace noir
