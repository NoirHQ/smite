// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/cli_helper.h>

namespace CLI::detail {

template<>
bool lexical_conversion<noir::uint256_t, noir::uint256_t>(
  const std::vector<std::string>& strings, noir::uint256_t& output) {
  output = noir::uint256_t(strings[0]);
  return true;
}

} // namespace CLI::detail
