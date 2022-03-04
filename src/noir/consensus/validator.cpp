// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/merkle/tree.h>
#include <noir/consensus/validator.h>
#include <noir/core/codec.h>

namespace noir::consensus {

bytes validator_set::get_hash() {
  merkle::bytes_list items;
  for (const auto& val : validators) {
    auto bz = encode(val);
    items.push_back(bz);
  }
  return merkle::hash_from_bytes_list(items);
}

} // namespace noir::consensus
