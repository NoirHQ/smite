// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/config.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

genesis_doc rand_genesis_doc(config config_, int num_validators, bool rand_power, int64_t min_power) {
  // todo - implement the rest
  return genesis_doc{get_time(), config_.base.chain_id, 1};
}

} // namespace noir::consensus
