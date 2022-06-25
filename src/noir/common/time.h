// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/core/result.h>
#include <chrono>

namespace noir {

using tstamp = std::chrono::system_clock::duration::rep;

tstamp get_time();

std::string tstamp_to_localtime_str(tstamp);
std::string tstamp_to_str(tstamp);
std::string tstamp_to_genesis_str(tstamp);

Result<tstamp> genesis_time_to_tstamp(const std::string_view&);

} // namespace noir
