// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/chrono.h>

namespace noir {

const std::unordered_map<std::string_view, std::chrono::nanoseconds> unit_map = {
  {"ns", std::chrono::nanoseconds(1)},
  {"us", std::chrono::microseconds(1)},
  {"ms", std::chrono::milliseconds(1)},
  {"s", std::chrono::seconds(1)},
  {"m", std::chrono::minutes(1)},
  {"h", std::chrono::hours(1)},
};

} // namespace noir
