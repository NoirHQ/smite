// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>
#include <future>

namespace noir::consensus {

// FIXME : These types are temporary.

using sender_type = std::string;
using tx_id_type = bytes32;

struct tx {
  sender_type sender;
  std::optional<tx_id_type> _id;

  bytes data;
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

struct response_check_tx {
  std::future<bool> result;
  uint32_t code;
  std::string sender;
};

static constexpr uint32_t code_type_ok = 0;

struct response_deliver_tx {
  uint32_t code;
};

using response_deliver_txs = std::vector<response_deliver_tx>;

} // namespace noir::consensus
