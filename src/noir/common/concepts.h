// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <cstddef>
#include <type_traits>

namespace noir {

// FIXME: remove after refactoring
template<typename T>
concept integral = std::is_integral_v<T>;

template<typename T>
concept unsigned_integral = std::is_integral_v<T> && !std::is_signed_v<T>;

template<typename E>
concept enumeration = std::is_enum_v<E>;

template<typename T, typename U = std::remove_cv_t<T>>
concept Byte = std::is_same_v<U, char> || std::is_same_v<U, unsigned char> || std::is_same_v<U, std::byte>;

template<typename T, typename U = std::remove_cv_t<T>>
concept NonByte = !(std::is_same_v<U, char> || std::is_same_v<U, unsigned char> || std::is_same_v<U, std::byte>);
//

template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept UnsignedIntegral = std::is_integral_v<T> && !
std::is_signed_v<T>;

template<typename E>
concept Enumeration = std::is_enum_v<E>;

template<typename From, typename To>
concept ConvertibleTo = std::is_convertible_v<From, To> && requires { static_cast<To>(std::declval<From>()); };

template<typename T>
using Deref = std::remove_reference_t<std::remove_pointer_t<T>>;

template<typename T>
concept Pointer = std::is_pointer_v<T>;

template<typename T>
concept ConstPointer = Pointer<T> && std::is_const_v<Deref<T>>;

} // namespace noir
