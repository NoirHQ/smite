// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/types.h>

namespace eth {

struct log {
  bytes20 address;
  std::vector<bytes32> topics;
  bytes data;
  uint256_t block_number;
  bytes32 tx_hash;
  uint32_t tx_index;
  bytes32 block_hash;
  uint32_t index;
  bool removed;
};

} // namespace eth
