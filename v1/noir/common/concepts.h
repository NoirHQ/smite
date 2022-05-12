// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <cstddef>
#include <type_traits>

namespace noir {

template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept UnsignedIntegral = std::is_integral_v<T> && !std::is_signed_v<T>;

template<typename E>
concept Enumeration = std::is_enum_v<E>;

template<typename T, typename U = std::remove_cv_t<T>>
concept ByteT = std::is_same_v<U, char> || std::is_same_v<U, unsigned char> || std::is_same_v<U, std::byte>;

} // namespace noir
