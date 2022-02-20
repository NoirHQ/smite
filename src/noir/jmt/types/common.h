// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/expected.h>
#include <string>

namespace noir::jmt {

using error = std::string;

template<typename T>
using result = expected<T, error>;

using version = uint64_t;

} // namespace noir::jmt
