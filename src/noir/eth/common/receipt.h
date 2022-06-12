// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/eth/common/log.h>

namespace noir::eth {

using Bytes256 = BytesN<256>;

struct receipt {
  uint8_t type;
  uint64_t status;
  uint64_t cumulative_gas_used;
  Bytes256 bloom;
  std::vector<log> logs;
  Bytes32 tx_hash;
  Bytes20 contract_address;
  uint64_t gas_used;
  Bytes32 block_hash;
  uint256_t block_number;
  uint32_t transaction_index;
};

} // namespace noir::eth
