// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/inttypes.h>
#include <memory>

namespace noir::eth {

struct transaction {
  uint64_t nonce;
  uint256_t gas_price;
  uint64_t gas;
  Bytes20 to;
  uint256_t value;
  Bytes data;
  uint64_t v;
  Bytes32 r;
  Bytes32 s;

  uint256_t fee() const {
    // FIXME: wrap check_uint256_t instead of using it directly
    return boost::multiprecision::checked_uint256_t(gas_price) * gas;
  }
};

using transaction_p = std::shared_ptr<transaction>;

struct rpc_transaction : public transaction {
  Bytes32 block_hash;
  uint256_t block_number;
  uint64_t transaction_index;
};

} // namespace noir::eth
