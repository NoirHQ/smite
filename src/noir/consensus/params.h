// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/types.h>

namespace noir::consensus { // todo - move consensus somewhere (maybe under libs?)

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
};

}
