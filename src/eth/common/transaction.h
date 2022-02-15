// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/types.h>

namespace eth {

struct transaction {
  uint64_t nonce;
  uint256_t gas_price;
  uint64_t gas;
  bytes20 to;
  uint256_t value;
  bytes data;
  uint64_t v;
  bytes32 r;
  bytes32 s;

  uint256_t fee() const {
    // FIXME: wrap check_uint256_t instead of using it directly
    return boost::multiprecision::checked_uint256_t(gas_price) * gas;
  }
};

using transaction_p = std::shared_ptr<transaction>;

} // namespace eth
