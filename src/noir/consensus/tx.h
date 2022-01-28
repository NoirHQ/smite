// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {

struct tx {
  sender_type sender;
  std::optional<tx_id_type> _id;

  p2p::bytes data;
  uint64_t gas;
  uint64_t nonce;
  uint64_t height;

  tx_id_type id() {
    if (_id == std::nullopt) {
      _id = tx_id_type{}; // FIXME
    }
    return _id.value();
  }

  uint64_t size() const {
    return sizeof(*this) + data.size();
  }
};

using tx_ptr = std::shared_ptr<tx>;
using tx_ptrs = std::vector<tx_ptr>;

} // namespace noir::consensus
