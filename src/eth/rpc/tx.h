// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/types.h>

namespace eth::rpc {

struct tx {
  uint64_t nonce;
  uint256_t gas_price;
  uint64_t gas;
  bytes20 to;
  uint256_t value;
  bytes data;
  uint256_t v;
  bytes32 r;
  bytes32 s;
};

using tx_ptr = std::shared_ptr<tx>;

} // namespace eth::rpc
