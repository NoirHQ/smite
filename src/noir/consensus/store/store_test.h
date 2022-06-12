// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/rand.h>
#include <noir/db/rocks_session.h>
#include <noir/db/session.h>

inline std::shared_ptr<rocksdb::DB> make_rocks_db(bool destroy = true, const std::string& name = "/tmp/testdb") {
  if (destroy) {
    rocksdb::DestroyDB(name.c_str(), rocksdb::Options{});
  }

  rocksdb::DB* cache_ptr{nullptr};
  auto cache = std::shared_ptr<rocksdb::DB>{};

  auto options = rocksdb::Options{};
  options.create_if_missing = true;
  options.level_compaction_dynamic_level_bytes = true;
  options.bytes_per_sync = 1048576;
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction(256ull << 20);

  auto status = rocksdb::DB::Open(options, name.c_str(), &cache_ptr);
  cache.reset(cache_ptr);

  return cache;
}

inline std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>> make_session(
  bool destroy = true, const std::string& name = "/tmp/testdb") {
  auto rocksdb = make_rocks_db(destroy, name);
  return std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(std::move(rocksdb), 16);
}

inline noir::Bytes gen_random_bytes(size_t num) {
  noir::Bytes ret(num);
  noir::crypto::rand_bytes(ret);
  return ret;
}
