// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/block.h>
#include <noir/consensus/db/db.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

class block_meta;

class block_store {
public:
  explicit block_store(const std::string& db_type = "simple")
    : _db(new noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes>) {}

  block_store(block_store&& other) noexcept : _db(std::move(other._db)) {}

  int64_t base() const {
    return 0;
  }

  int64_t height() const {
    return 0;
  }

  int64_t size() const {
    return 0;
  }

  block_meta* load_block_meta(int64_t height) const {
    return nullptr;
  }

  block* load_block(int64_t height) const {
    return nullptr;
  }

  void save_block(block* bl, part_set* ps, commit* seen_commit) {}

  bool prune_blocks(int64_t height) {
    return true;
  }

  block* load_block_by_hash(const noir::p2p::bytes& hash) const {
    return nullptr;
  }

  part* load_block_part(int64_t height, int index) const {
    return nullptr;
  }

  commit* load_block_commit(int64_t height) const {
    return nullptr;
  }

  commit* load_seen_commit() const {
    return nullptr;
  }

private:
  std::unique_ptr<noir::consensus::db<noir::p2p::bytes, noir::p2p::bytes>> _db;
};

} // namespace noir::consensus