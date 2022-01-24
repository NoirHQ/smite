// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/abci.h>
#include <noir/consensus/tx.h>
#include <noir/tx_pool/unapplied_tx_queue.hpp>

namespace noir::tx_pool {
class tx_pool {
public:
  struct config {
    std::string version = "v0.0.1";
    std::string root_dir = "/";
    bool recheck = true;
    bool broadcast = true;
    uint32_t size = 5000;
    uint64_t max_tx_bytes = 1024 * 1024;
    uint64_t max_txs_bytes = 1024 * 1024 * 1024;
    uint64_t max_batch_bytes;
    uint64_t cache_size = 10000;
    bool keep_invalid_txs_in_cache = false;
//    fc::time_point ttl_duration;
//    uint64_t ttl_num_blocks = 0;
  };

  using precheck_func = bool(const consensus::tx&);
  using postcheck_func = bool(const consensus::tx&, consensus::response_check_tx&);

private:
  std::mutex mutex_;
  config config_;
  unapplied_tx_queue tx_queue_;

  precheck_func* precheck_;
  postcheck_func* postcheck_;

public:
  tx_pool();
  tx_pool(const config& cfg);

  void set_precheck(precheck_func* precheck);
  void set_postcheck(postcheck_func* postcheck);

  bool check_tx(const consensus::tx_ptr& tx_ptr);
  consensus::tx_ptrs reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas);
  consensus::tx_ptrs reap_max_txs(uint64_t tx_count);
  bool update(uint64_t block_height, consensus::tx_ptrs& tx_ptrs);

  size_t size() const;
  void flush();
};

} // namespace noir::tx_pool
