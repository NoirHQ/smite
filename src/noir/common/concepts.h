// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <type_traits>

namespace noir {

template<typename T>
concept integral = std::is_integral_v<T>;

template<typename E>
concept enumeration = std::is_enum_v<E>;

template<typename T, typename U = std::remove_cv_t<T>>
concept Byte = std::is_same_v<U, char> || std::is_same_v<U, unsigned char> || std::is_same_v<U, std::byte>;

template<typename T, typename U = std::remove_cv_t<T>>
concept NonByte = !(std::is_same_v<U, char> || std::is_same_v<U, unsigned char> || std::is_same_v<U, std::byte>);

} // namespace noir
