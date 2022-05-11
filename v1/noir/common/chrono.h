// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <chrono>

namespace noir {

auto parse_duration(std::string_view s) -> Result<std::chrono::steady_clock::duration>;

} // namespace noir
