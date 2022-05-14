// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace noir {

template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept UnsignedIntegral = std::is_integral_v<T> && !std::is_signed_v<T>;

template<typename E>
concept Enumeration = std::is_enum_v<E>;

template<typename From, typename To>
concept ConvertibleTo = std::is_convertible_v<From, To> && requires {
  static_cast<To>(std::declval<From>());
};

template<typename T>
using Deref = std::remove_cvref_t<std::remove_pointer_t<T>>;

} // namespace noir
