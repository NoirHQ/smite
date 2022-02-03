// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <noir/db/rocks_session.hpp>
#include <noir/db/session.hpp>

inline std::shared_ptr<rocksdb::DB> make_rocks_db(const std::string& name = "/tmp/testdb") {
  rocksdb::DestroyDB(name.c_str(), rocksdb::Options{});

  rocksdb::DB* cache_ptr{nullptr};
  auto cache = std::shared_ptr<rocksdb::DB>{};

  auto options = rocksdb::Options{};
  options.create_if_missing = true;
  options.level_compaction_dynamic_level_bytes = true;
  options.bytes_per_sync = 1048576;
  options.OptimizeLevelStyleCompaction(256ull << 20);

  auto status = rocksdb::DB::Open(options, name.c_str(), &cache_ptr);
  cache.reset(cache_ptr);

  return cache;
}

inline noir::db::session::session<noir::db::session::rocksdb_t> make_session(const std::string& name = "/tmp/testdb") {
  auto rocksdb = make_rocks_db(name);
  return noir::db::session::make_session(std::move(rocksdb), 16);
}

inline noir::bytes gen_random_bytes(size_t num) {
  noir::bytes ret(num);
  fc::rand_pseudo_bytes(ret.data(), ret.size());
  return ret;
}
