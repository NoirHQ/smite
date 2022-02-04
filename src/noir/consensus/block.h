// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/common/hex.h>
#include <noir/common/types.h>
#include <noir/consensus/tx.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

#include <fc/crypto/rand.hpp>

namespace noir::consensus {

struct block_data {
  std::vector<tx> txs;
  bytes hash;
};

struct block_header {
  int64_t height{};
  p2p::tstamp time{};

  bytes32 hash_; // todo - remove later after properly compute hash

  bytes get_hash() {
    // todo - properly compute hash
    if (hash_ == bytes32())
      fc::rand_pseudo_bytes(hash_.data(), hash_.data_size());
    return from_hex(hash_.str());
  }
};

struct block {
  // mutex mtx;
  block_header header;
  block_data data;
  // evidence evidence;
  // commit last_commit;

  bytes get_hash() {
    // todo - lock mtx
    // todo - implement
    return header.get_hash();
  }

  bool hashes_to(bytes hash) {
    if (hash.empty())
      return false;
    return get_hash() == hash;
  }
};

enum block_id_flag {
  FlagAbsent = 1,
  FlagCommit,
  FlagNil
};

struct commit_sig {
  block_id_flag flag;
  bytes validator_address;
  p2p::tstamp timestamp;
  bytes signature;
  p2p::vote_extension_to_sign vote_extension;

  static commit_sig new_commit_sig_absent() {
    return commit_sig{FlagAbsent};
  }

  bool for_block() {
    return flag == FlagCommit;
  }

  bool absent() {
    return flag == FlagAbsent;
  }
};

struct commit {
  int64_t height;
  int32_t round;
  p2p::block_id my_block_id;
  std::vector<commit_sig> signatures;

  // todo - do we need these?
  //  hash
  //  bitArray

  static commit new_commit(
    int64_t height_, int32_t round_, p2p::block_id block_id_, std::vector<commit_sig>& commit_sigs) {
    return commit{height_, round_, block_id_, commit_sigs};
  }
};

using block_ptr = std::shared_ptr<block>;

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(
  noir::consensus::commit_sig, /* TODO: flag, */ validator_address, timestamp, signature, vote_extension);
NOIR_FOR_EACH_FIELD(noir::consensus::commit, height, round, my_block_id, signatures);
