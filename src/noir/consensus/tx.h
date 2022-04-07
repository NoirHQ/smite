// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>

namespace noir::consensus {

// struct tx {};
using tx = bytes;

using tx_ptr = std::shared_ptr<tx>;

using address_type = bytes;
using tx_hash = bytes32;

static tx_hash get_tx_hash(const tx& tx) {
  if (tx.size() == 0) {
    return tx_hash{};
  }
  crypto::sha3_256 hash;
  return tx_hash{hash(tx)}; // FIXME
}

struct wrapped_tx {
  // TODO : constructor
  address_type sender;
  tx_hash hash;
  consensus::tx tx;
  uint64_t gas;
  uint64_t nonce;
  uint64_t height;
  p2p::tstamp time_stamp;

  uint64_t size() const {
    return sizeof(*this) + tx.size();
  }
};

using wrapped_tx_ptr = std::shared_ptr<wrapped_tx>;
using wrapped_tx_ptrs = std::vector<wrapped_tx_ptr>;

} // namespace noir::consensus
