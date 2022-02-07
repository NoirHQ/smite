// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/inttypes.h>
#include <appbase/CLI11.hpp>

namespace CLI::detail {

template<>
struct expected_count<noir::uint256_t> {
  static constexpr int value = 1;
};

template<>
bool lexical_conversion<noir::uint256_t, noir::uint256_t>(
  const std::vector<std::string>& strings, noir::uint256_t& output);

} // namespace CLI::detail
