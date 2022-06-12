// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/inttypes.h>

namespace noir::eth {

struct log {
  Bytes20 address;
  std::vector<Bytes32> topics;
  Bytes data;
  uint256_t block_number;
  Bytes32 tx_hash;
  uint32_t tx_index;
  Bytes32 block_hash;
  uint32_t index;
  bool removed;
};

} // namespace noir::eth
