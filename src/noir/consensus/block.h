// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>

namespace noir::consensus { // todo - where to place?

using namespace noir::p2p;

struct block {
  //mutex mtx;
  //header header;
  //data data;
  //evidence evidence;
  //commit last_commit;
};

struct vote_extension_to_sign {
  bytes add_data_to_sign;
};

struct commit_sig {
  std::byte block_id_flag;
  bytes validator_address;
  tstamp timestamp;
  bytes signature;
  vote_extension_to_sign vote_extension;
};

struct commit {
  int64_t height;
  int32_t round;
  block_id my_block_id;
  std::vector<commit_sig> signatures;

  // todo - do we need these?
//  hash
//  bitArray
};

using block_ptr = std::shared_ptr<block>;

}

