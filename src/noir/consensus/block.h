// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

struct block {
  // mutex mtx;
  // header header;
  // data data;
  // evidence evidence;
  // commit last_commit;
};

struct vote_extension_to_sign {
  p2p::bytes add_data_to_sign;
};

struct commit_sig {
  std::byte block_id_flag;
  p2p::bytes validator_address;
  p2p::tstamp timestamp;
  p2p::bytes signature;
  vote_extension_to_sign vote_extension;
};

struct commit {
  int64_t height;
  int32_t round;
  p2p::block_id my_block_id;
  std::vector<commit_sig> signatures;

  // todo - do we need these?
  //  hash
  //  bitArray
};

using block_ptr = std::shared_ptr<block>;

} // namespace noir::consensus
