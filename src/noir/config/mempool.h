// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <CLI/CLI11.hpp>

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

  void bind(CLI::App& cmd) {
    auto mempool = cmd.add_section("mempool");
    mempool->add_option("version", version);
    mempool->add_option("recheck", recheck);
    mempool->add_option("broadcast", broadcast);
    mempool->add_option("size", size);
    mempool->add_option("max-txs-bytes", max_txs_bytes);
    mempool->add_option("cache-size", cache_size);
    mempool->add_option("keep-invalid-txs-in-cache", keep_invalid_txs_in_cache);
    mempool->add_option("max-tx-bytes", max_tx_bytes);
    mempool->add_option("max-batch-bytes", max_batch_bytes);
    mempool->add_option("ttl-duration", ttl_duration);
    mempool->add_option("ttl-num-blocks", ttl_num_blocks);
  }
};

} // namespace noir::config
