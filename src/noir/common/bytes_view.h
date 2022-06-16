// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/concepts.h>
#include <cstring>
#include <span>

namespace noir {

// concept for byte compatible type
// template<typename T, typename U = std::remove_cv_t<T>>
// concept Byte = std::is_same_v<U, unsigned char> || std::is_same_v<U, char> || std::is_same_v<U, std::byte>;

// concept for a pointer to byte compatible type
template<typename T>
concept BytePointer = Pointer<T> && Byte<Deref<T>>;

template<typename T>
concept ByteSequence = requires(T v) {
  { v.data() } -> BytePointer;
  { v.size() } -> ConvertibleTo<size_t>;
};

// concept for convertible to bytes view (no implicit)
template<typename T>
concept BytesViewConstructible = !std::is_convertible_v<T, std::span<const unsigned char>> && ByteSequence<T>;

auto byte_pointer_cast(Pointer auto pointer) {
  if constexpr (ConstPointer<decltype(pointer)>) {
    return reinterpret_cast<const unsigned char*>(pointer);
  } else {
    return reinterpret_cast<unsigned char*>(pointer);
  }
}

auto bytes_view(ByteSequence auto& bytes) {
  return std::span(byte_pointer_cast(bytes.data()), bytes.size());
}

inline bool operator==(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  return !std::memcmp(lhs.data(), rhs.data(), lhs.size());
}

inline bool operator!=(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  if (lhs.size() != rhs.size()) {
    return true;
  }
  return std::memcmp(lhs.data(), rhs.data(), lhs.size());
}

inline bool operator<(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  auto cmp = std::memcmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
  if (cmp < 0) {
    return true;
  } else if (!cmp) {
    return lhs.size() < rhs.size();
  }
  return false;
}

inline bool operator<=(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  auto cmp = std::memcmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
  if (cmp < 0) {
    return true;
  } else if (!cmp) {
    return lhs.size() <= rhs.size();
  }
  return false;
}

inline bool operator>(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  auto cmp = std::memcmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
  if (cmp > 0) {
    return true;
  } else if (!cmp) {
    return lhs.size() > rhs.size();
  }
  return false;
}

inline bool operator>=(std::span<const unsigned char> lhs, std::span<const unsigned char> rhs) {
  auto cmp = std::memcmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
  if (cmp > 0) {
    return true;
  } else if (!cmp) {
    return lhs.size() >= rhs.size();
  }
  return false;
}

} // namespace noir
