// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/hash.h>
#include <unordered_map>

namespace noir {

template<typename K, typename V>
using unordered_map = std::unordered_map<K, V, noir::hash<K>>;

} // namespace noir
