// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <appbase/CLI11.hpp>

namespace noir::config {

struct MempoolConfig {
  std::string version;
  std::string root_dir;
  bool recheck;
  bool broadcast;
  int size;
  int64_t max_txs_bytes;
  int cache_size;
  bool keep_invalid_txs_in_cache;
  int max_tx_bytes;
  int max_batch_bytes;
  std::chrono::system_clock::duration ttl_duration;
  int64_t ttl_num_blocks;

  MempoolConfig() {
    version = "v1";
    recheck = true;
    broadcast = true;
    size = 5000;
    max_txs_bytes = 1024 * 1024 * 1024; // 1GB
    cache_size = 10000;
    keep_invalid_txs_in_cache = false;
    max_tx_bytes = 1024 * 1024; // 1MB
    ttl_duration = std::chrono::seconds(0);
    ttl_num_blocks = 0;
  }

  void bind(CLI::App& root);
};

} // namespace noir::config
