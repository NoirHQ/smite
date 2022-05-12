// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/helper/rust.h>
#include <chrono>

namespace noir {

using tstamp = std::chrono::system_clock::duration::rep;

tstamp get_time();
result<std::time_t> parse_genesis_time(const char* time_str);

} // namespace noir
