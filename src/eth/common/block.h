// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/bloom.h>
#include <eth/common/transaction.h>
#include <eth/common/types.h>

namespace eth {

struct header {
  bytes32 parent_hash;
  bytes32 uncle_hash;
  bytes20 coinbase;
  bytes32 root;
  bytes32 tx_hash;
  bytes32 receipt_hash;
  bloom bloom;
  uint256_t difficulty;
  uint256_t number;
  uint64_t gas_limit;
  uint64_t gas_used;
  uint64_t time;
  bytes extra;
  bytes32 mix_digest;
  uint64_t nonce;
  uint256_t base_fee;
};

struct block {
  header header;
  std::vector<class header> uncles;
  std::vector<transaction> transactions;
  bytes32 hash;
  uint64_t size;
  uint256_t td;
};

} // namespace eth
