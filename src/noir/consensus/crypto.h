// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/p2p/protocol.h>

namespace noir::consensus {
struct pub_key {
  bytes key;

  bool empty() {
    return key.empty();
  }

  bytes address() {
    // todo - check key length
    if (key.empty())
      throw std::runtime_error("pub_key: unable to get address as key is empty");
    return key; // todo - convert address from key
  }
};

} // namespace noir::consensus
