// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/types.h>

namespace noir::consensus {

constexpr int64_t max_block_size_bytes{104857600};

struct block_params {
  int64_t max_bytes;
  int64_t max_gas;
};

struct evidence_params {
  int64_t max_age_num_blocks;
  int64_t max_age_duration; // todo - use duration
  int64_t max_bytes;
};

struct validator_params {
  std::vector<std::string> pub_key_types;
};

struct version_params {
  uint64_t app_version;
};

struct consensus_params {
  block_params block;
  evidence_params evidence;
  validator_params validator;
  version_params version;

  std::optional<std::string> validate_consensus_params() const {
    if (block.max_bytes <= 0)
      return "block.MaxBytes must be greater than 0.";
    if (block.max_bytes > max_block_size_bytes)
      return "block.MaxBytes is too big.";
    if (block.max_gas < -1)
      return "block.MaxGas must be greater or equal to -1.";
    // check evidence // todo - necessary?
    // if (validator.pub_key_types.empty())
    //  return "validator.pub_key_types must not be empty.";
    // check if key_type is known // todo
    return {};
  }

  bytes hash_consensus_params() {
    // todo
    return bytes{};
  }
};

} // namespace noir::consensus
