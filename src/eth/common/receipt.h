// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/log.h>
#include <eth/common/types.h>

namespace eth {

struct receipt {
  uint8_t type;
  uint64_t status;
  uint64_t cumulative_gas_used;
  bytes256 bloom;
  std::vector<log> logs;
  bytes32 tx_hash;
  bytes20 transaction_hash;
  uint64_t gas_used;
  bytes32 block_hash;
  uint256_t block_number;
  uint32_t transaction_index;
};

} // namespace eth