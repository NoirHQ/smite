// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>

namespace eth::rpc {

struct tx {
  uint64_t nonce;
  noir::uint256_t gas_price;
  uint64_t gas;
  noir::bytes20 to;
  noir::uint256_t value;
  noir::bytes data;
  uint8_t v;
  noir::bytes32 r;
  noir::bytes32 s;
};

using tx_ptr = std::shared_ptr<tx>;

} // namespace eth::rpc
