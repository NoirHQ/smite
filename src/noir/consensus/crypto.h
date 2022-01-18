// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/consensus/types.h>

namespace noir::consensus {
struct pub_key {
  p2p::bytes key;

  bool empty() {
    return key.empty();
  }

  p2p::bytes address() {
    // todo - check key length
    return std::vector<char>(from_hex("000000"));
  }
};

} // namespace noir::consensus
