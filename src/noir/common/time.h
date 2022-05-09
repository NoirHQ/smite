// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <chrono>

namespace noir {

using tstamp = std::chrono::system_clock::duration::rep;

tstamp get_time();

} // namespace noir
