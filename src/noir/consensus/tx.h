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
using tx_id_type = bytes32;

struct wrapped_tx {
  address_type sender;
  std::optional<tx_id_type> _id;

  tx tx_data;
  uint64_t gas;
  uint64_t nonce;
  uint64_t height;
  p2p::tstamp time_stamp;

  tx_id_type id() {
    if (!_id.has_value()) {
      _id = tx_id_type{}; // FIXME
    }
    return _id.value();
  }

  uint64_t size() const {
    return sizeof(*this) + tx_data.size();
  }
};

using wrapped_tx_ptr = std::shared_ptr<wrapped_tx>;
using wrapped_tx_ptrs = std::vector<wrapped_tx_ptr>;

} // namespace noir::consensus
