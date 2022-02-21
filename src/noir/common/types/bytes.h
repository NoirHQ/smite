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
#include <fmt/format.h>
#include <span>

namespace noir::codec {
template<typename T>
struct basic_datastream;
}

namespace noir {

using byte_type = char;

using bytes = std::vector<byte_type>;

/// \brief thin wrapper for fixed-length byte array
/// \ingroup common
template<size_t N, Byte T = byte_type>
struct bytesN {
  std::array<T, N> data_;

  using underlying_type = decltype(data_);
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

  constexpr bytesN() = default;

  constexpr bytesN(std::string_view s, bool canonical = false) {
    auto size = from_hex(s, data_);
    check(!canonical || size == N, fmt::format("invalid bytes length: expected({}), actual({})", N, size));
    if (size < N) {
      std::fill(data_.begin() + size, data_.end(), 0);
    }
  }

  template<Byte U>
  constexpr bytesN(std::span<U> bytes, bool canonical = false) {
    auto size = bytes.size();
    check(!canonical || size == N, fmt::format("invalid bytes length: expected({}), actual({})", N, size));
    std::copy(bytes.begin(), bytes.end(), data_.begin());
    if (size < N) {
      std::fill(data_.begin() + size, data_.end(), 0);
    }
  }

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
  [[nodiscard]] constexpr bool empty() const noexcept {
    return data_.empty();
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

  std::span<T> to_span() {
    return std::span{data_.data(), data_.size()};
  }

  std::span<const T> to_span() const {
    return std::span{data_.data(), data_.size()};
  }

  void clear() {
    std::fill(data_.begin(), data_.end(), 0);
  }
};

template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator==(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  if constexpr (N1 != N2)
    return false;
  return !std::memcmp(a.data_.data(), b.data_.data(), N1);
}
template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator!=(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  if constexpr (N1 != N2)
    return true;
  return std::memcmp(a.data_.data(), b.data_.data(), N1);
}

template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator<(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  auto cmp = std::memcmp(a.data_.data(), b.data_.data(), std::min(N1, N2));
  if (cmp < 0) {
    return true;
  } else if (!cmp) {
    return N1 < N2;
  }
  return false;
}

template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator<=(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  auto cmp = std::memcmp(a.data_.data(), b.data_.data(), std::min(N1, N2));
  if (cmp < 0) {
    return true;
  } else if (!cmp) {
    return N1 <= N2;
  }
  return false;
}

template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator>(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  auto cmp = std::memcmp(a.data_.data(), b.data_.data(), std::min(N1, N2));
  if (cmp > 0) {
    return true;
  } else if (!cmp) {
    return N1 > N2;
  }
  return false;
}
template<size_t N1, size_t N2, Byte T1, Byte T2>
inline bool operator>=(const bytesN<N1, T1>& a, const bytesN<N2, T2>& b) {
  auto cmp = std::memcmp(a.data_.data(), b.data_.data(), std::min(N1, N2));
  if (cmp > 0) {
    return true;
  } else if (!cmp) {
    return N1 >= N2;
  }
  return false;
}

template<typename DataStream, size_t N, Byte U = byte_type>
DataStream& operator<<(DataStream& ds, const bytesN<N, U>& v) {
  ds << v.to_span();
  return ds;
}

template<typename DataStream, size_t N, Byte U = byte_type>
DataStream& operator>>(DataStream& ds, bytesN<N, U>& v) {
  ds >> v.to_span();
  return ds;
}

template<size_t N, Byte U = byte_type>
std::ostream& operator<<(std::ostream& ds, const bytesN<N, U>& v) {
  ds.write(v.data(), v.size());
  return ds;
}

template<size_t N, Byte U = byte_type>
std::istream& operator>>(std::istream& ds, bytesN<N, U>& v) {
  ds.read(v.data(), v.size());
  return ds;
}

using bytes20 = bytesN<20>;
using bytes32 = bytesN<32>;

template<>
struct is_foreachable<bytes20> : std::false_type {};
template<>
struct is_foreachable<bytes32> : std::false_type {};

} // namespace noir
