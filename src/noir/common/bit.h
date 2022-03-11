// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <cstdint>

namespace noir {

template<typename T>
constexpr T byteswap(T v) noexcept;

template<>
constexpr uint16_t byteswap(uint16_t v) noexcept {
  return __builtin_bswap16(v);
}

template<>
constexpr uint32_t byteswap(uint32_t v) noexcept {
  return __builtin_bswap32(v);
}

template<>
constexpr uint64_t byteswap(uint64_t v) noexcept {
  return __builtin_bswap64(v);
}

} // namespace noir