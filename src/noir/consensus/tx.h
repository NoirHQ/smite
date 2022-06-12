// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>

namespace noir::consensus {

// struct tx {};
using tx = Bytes;

using tx_ptr = std::shared_ptr<tx>;

using address_type = Bytes;
using tx_hash = Bytes32;

static tx_hash get_tx_hash(const tx& tx) {
  if (tx.size() == 0) {
    return tx_hash{};
  }
  crypto::Sha3_256 hash;
  return tx_hash{hash(tx)}; // FIXME
}

struct wrapped_tx {
  // TODO : constructor
  address_type sender;
  tx_hash hash;
  consensus::tx_ptr tx_ptr;
  uint64_t gas;
  uint64_t nonce;
  uint64_t height;
  tstamp time_stamp;

  wrapped_tx() = delete;
  wrapped_tx(address_type& sender,
    const consensus::tx_ptr& tx_ptr,
    uint64_t gas = 0,
    uint64_t nonce = 0,
    uint64_t height = 0,
    tstamp time_stamp = noir::get_time())
    : sender(sender),
      hash(get_tx_hash(*tx_ptr)),
      tx_ptr(tx_ptr),
      gas(gas),
      nonce(nonce),
      height(height),
      time_stamp(time_stamp) {}
  wrapped_tx(wrapped_tx const& rhs) = default;
  wrapped_tx(wrapped_tx&& rhs) = default;

  uint64_t size() const {
    return sizeof(*this) + tx_ptr->size();
  }
};

using wrapped_tx_ptr = std::shared_ptr<wrapped_tx>;
using wrapped_tx_ptrs = std::vector<wrapped_tx_ptr>;

} // namespace noir::consensus
