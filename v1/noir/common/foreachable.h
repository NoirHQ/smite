// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <type_traits>

namespace noir {

template<typename T>
struct IsForeachable : std::false_type {};

template<typename T>
constexpr bool is_foreachable_v = IsForeachable<T>::value;

template<typename T>
concept Foreachable = is_foreachable_v<T>;

} // namespace noir
